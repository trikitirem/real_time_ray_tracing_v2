#include "renderer/ray_tracing/blas_builder.hpp"

#include "renderer/ray_tracing/rt_scene_gpu_data.hpp"
#include "renderer/shared/device_context.hpp"
#include "util/vertex.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace renderer::ray_tracing {

namespace {

std::uint32_t find_memory_type(const vk::raii::PhysicalDevice& physical,
                               std::uint32_t type_bits,
                               vk::MemoryPropertyFlags required)
{
    const vk::PhysicalDeviceMemoryProperties mem = physical.getMemoryProperties();
    for (std::uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        const bool supported = (type_bits & (1u << i)) != 0;
        const bool has_props = (mem.memoryTypes[i].propertyFlags & required) == required;
        if (supported && has_props) {
            return i;
        }
    }
    throw std::runtime_error("BlasBuilder: no suitable memory type");
}

void create_buffer(const vk::raii::PhysicalDevice& physical,
                   const vk::raii::Device& device,
                   vk::DeviceSize size,
                   vk::BufferUsageFlags usage,
                   vk::MemoryPropertyFlags properties,
                   vk::raii::Buffer& out_buffer,
                   vk::raii::DeviceMemory& out_memory)
{
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;
    out_buffer = vk::raii::Buffer(device, buffer_info);

    const vk::MemoryRequirements req = out_buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical, req.memoryTypeBits, properties);

    vk::MemoryAllocateFlagsInfo flags_info{};
    if ((usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) != vk::BufferUsageFlags{}) {
        flags_info.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
        alloc_info.pNext = &flags_info;
    }

    out_memory = vk::raii::DeviceMemory(device, alloc_info);
    out_buffer.bindMemory(*out_memory, 0);
}

std::unique_ptr<vk::raii::CommandBuffer> begin_single_time_commands(const vk::raii::Device& device,
                                                                     const vk::raii::CommandPool& command_pool)
{
    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = *command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;
    auto cmd = std::make_unique<vk::raii::CommandBuffer>(
        std::move(vk::raii::CommandBuffers(device, alloc_info).front()));
    cmd->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    return cmd;
}

void end_single_time_commands(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cmd)
{
    cmd.end();
    const vk::CommandBuffer raw = *cmd;
    queue.submit(vk::SubmitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &raw,
    }, nullptr);
    queue.waitIdle();
}

} // namespace

BlasBuildResult BlasBuilder::build(DeviceContext& ctx, const RtSceneGpuData& scene_data) const
{
    BlasBuildResult out{};
    if (scene_data.draw_items.empty() && scene_data.instanced_items.empty()) {
        return out;
    }

    const vk::raii::Device& device = ctx.device();
    const vk::raii::PhysicalDevice& physical = ctx.physicalDevice();

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = ctx.graphicsQueueFamily();
    const vk::raii::CommandPool command_pool(device, pool_info);

    // Count total TLAS instances up-front for reservation.
    std::uint32_t total_instanced = 0;
    for (const RtInstancedDrawItem& g : scene_data.instanced_items) {
        total_instanced += static_cast<std::uint32_t>(g.transforms.size());
    }
    out.entries.reserve(scene_data.draw_items.size() + scene_data.instanced_items.size());
    out.instance_inputs.reserve(scene_data.draw_items.size() + total_instanced);

    for (std::uint32_t i = 0; i < scene_data.draw_items.size(); ++i) {
        const RtDrawItem& item = scene_data.draw_items[i];
        if (item.index_count == 0) {
            continue;
        }

        vk::BufferDeviceAddressInfo vertex_addr_info{ .buffer = item.vertex_buffer };
        const vk::DeviceAddress vertex_addr = device.getBufferAddress(vertex_addr_info);
        vk::BufferDeviceAddressInfo index_addr_info{ .buffer = item.index_buffer };
        const vk::DeviceAddress index_addr = device.getBufferAddress(index_addr_info);

        const std::uint32_t primitive_count = item.index_count / 3;
        const std::uint32_t vertex_count = std::max(1u, item.vertex_count);
        const std::uint32_t max_vertex = vertex_count - 1;

        auto triangles = vk::AccelerationStructureGeometryTrianglesDataKHR{
            .vertexFormat = vk::Format::eR32G32B32Sfloat,
            .vertexData = vertex_addr,
            .vertexStride = sizeof(util::Vertex),
            .maxVertex = max_vertex,
            .indexType = vk::IndexType::eUint32,
            .indexData = index_addr + static_cast<vk::DeviceSize>(item.first_index) * sizeof(std::uint32_t),
        };
        vk::AccelerationStructureGeometryDataKHR geometry_data(triangles);
        vk::AccelerationStructureGeometryKHR geometry{
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry = geometry_data,
            .flags = vk::GeometryFlagBitsKHR::eOpaque,
        };

        vk::AccelerationStructureBuildGeometryInfoKHR build_info{
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
            .geometryCount = 1,
            .pGeometries = &geometry,
        };

        const vk::AccelerationStructureBuildSizesInfoKHR sizes =
            device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                build_info,
                { primitive_count });

        vk::raii::Buffer scratch_buffer = nullptr;
        vk::raii::DeviceMemory scratch_memory = nullptr;
        create_buffer(
            physical, device, sizes.buildScratchSize,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            scratch_buffer, scratch_memory);
        const vk::DeviceAddress scratch_addr =
            device.getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = *scratch_buffer });
        build_info.scratchData.deviceAddress = scratch_addr;

        BlasEntry entry{};
        create_buffer(
            physical, device, sizes.accelerationStructureSize,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            entry.buffer, entry.memory);

        entry.handle = device.createAccelerationStructureKHR(vk::AccelerationStructureCreateInfoKHR{
            .buffer = *entry.buffer,
            .offset = 0,
            .size = sizes.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        });
        build_info.dstAccelerationStructure = *entry.handle;

        const std::uint32_t first_vertex = static_cast<std::uint32_t>(std::max(0, item.vertex_offset));
        vk::AccelerationStructureBuildRangeInfoKHR range_info{
            .primitiveCount = primitive_count,
            .primitiveOffset = 0,
            .firstVertex = first_vertex,
            .transformOffset = 0,
        };

        auto cmd = begin_single_time_commands(device, command_pool);
        cmd->buildAccelerationStructuresKHR({ build_info }, { &range_info });
        end_single_time_commands(ctx.graphicsQueue(), *cmd);

        const vk::DeviceAddress blas_addr =
            device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR{
                .accelerationStructure = *entry.handle,
            });

        out.instance_inputs.push_back(BlasInstanceInput{
            .geometry_index = i,
            .blas_address = blas_addr,
            .material_index = item.material_index,
            .index_offset = item.first_index,
            .model_matrix = item.model_matrix,
        });
        out.entries.push_back(std::move(entry));
    }

    // Instanced groups: one BLAS per prototype, N TLAS entries per group.
    // geometry_index starts after all legacy draw_items so the RT shader
    // can index into the combined reflection_instance_lut correctly.
    std::uint32_t instanced_geometry_base
        = static_cast<std::uint32_t>(scene_data.draw_items.size());

    for (const RtInstancedDrawItem& item : scene_data.instanced_items) {
        if (item.index_count == 0 || item.transforms.empty()) {
            continue;
        }

        vk::BufferDeviceAddressInfo vertex_addr_info{ .buffer = item.vertex_buffer };
        const vk::DeviceAddress vertex_addr = device.getBufferAddress(vertex_addr_info);
        vk::BufferDeviceAddressInfo index_addr_info{ .buffer = item.index_buffer };
        const vk::DeviceAddress index_addr = device.getBufferAddress(index_addr_info);

        const std::uint32_t primitive_count = item.index_count / 3;
        const std::uint32_t max_vertex = std::max(1u, item.vertex_count) - 1;

        auto triangles = vk::AccelerationStructureGeometryTrianglesDataKHR{
            .vertexFormat = vk::Format::eR32G32B32Sfloat,
            .vertexData   = vertex_addr,
            .vertexStride = sizeof(util::Vertex),
            .maxVertex    = max_vertex,
            .indexType    = vk::IndexType::eUint32,
            .indexData    = index_addr,
        };
        vk::AccelerationStructureGeometryDataKHR geometry_data(triangles);
        vk::AccelerationStructureGeometryKHR geometry{
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry     = geometry_data,
            .flags        = vk::GeometryFlagBitsKHR::eOpaque,
        };

        vk::AccelerationStructureBuildGeometryInfoKHR build_info{
            .type          = vk::AccelerationStructureTypeKHR::eBottomLevel,
            .mode          = vk::BuildAccelerationStructureModeKHR::eBuild,
            .geometryCount = 1,
            .pGeometries   = &geometry,
        };

        const vk::AccelerationStructureBuildSizesInfoKHR sizes =
            device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                build_info, { primitive_count });

        vk::raii::Buffer scratch_buffer = nullptr;
        vk::raii::DeviceMemory scratch_memory = nullptr;
        create_buffer(physical, device, sizes.buildScratchSize,
            vk::BufferUsageFlagBits::eStorageBuffer
                | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            scratch_buffer, scratch_memory);
        build_info.scratchData.deviceAddress =
            device.getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = *scratch_buffer });

        BlasEntry entry{};
        create_buffer(physical, device, sizes.accelerationStructureSize,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            entry.buffer, entry.memory);

        entry.handle = device.createAccelerationStructureKHR(
            vk::AccelerationStructureCreateInfoKHR{
                .buffer = *entry.buffer,
                .offset = 0,
                .size   = sizes.accelerationStructureSize,
                .type   = vk::AccelerationStructureTypeKHR::eBottomLevel,
            });
        build_info.dstAccelerationStructure = *entry.handle;

        vk::AccelerationStructureBuildRangeInfoKHR range_info{
            .primitiveCount  = primitive_count,
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0,
        };

        auto cmd = begin_single_time_commands(device, command_pool);
        cmd->buildAccelerationStructuresKHR({ build_info }, { &range_info });
        end_single_time_commands(ctx.graphicsQueue(), *cmd);

        const vk::DeviceAddress blas_addr =
            device.getAccelerationStructureAddressKHR(
                vk::AccelerationStructureDeviceAddressInfoKHR{
                    .accelerationStructure = *entry.handle });

        // N TLAS instances — all share the same blas_addr, each gets a unique
        // geometry_index so the RT shader can find its LUT entry.
        for (std::uint32_t t = 0;
             t < static_cast<std::uint32_t>(item.transforms.size()); ++t) {
            out.instance_inputs.push_back(BlasInstanceInput{
                .geometry_index = instanced_geometry_base + t,
                .blas_address   = blas_addr,
                .material_index = item.material_index,
                .index_offset   = 0,
                .model_matrix   = item.transforms[t],
            });
        }
        instanced_geometry_base
            += static_cast<std::uint32_t>(item.transforms.size());

        out.entries.push_back(std::move(entry));
    }

    return out;
}

} // namespace renderer::ray_tracing
