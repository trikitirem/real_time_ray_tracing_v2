#include "renderer/ray_tracing/tlas_builder.hpp"

#include "renderer/ray_tracing/blas_builder.hpp"
#include "renderer/shared/device_context.hpp"

#include <array>
#include <cstring>
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
    throw std::runtime_error("TlasBuilder: no suitable memory type");
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

TlasBuildResult TlasBuilder::build(DeviceContext& ctx, const std::vector<BlasInstanceInput>& instances) const
{
    TlasBuildResult out{};
    if (instances.empty()) {
        return out;
    }

    const vk::raii::Device& device = ctx.device();
    const vk::raii::PhysicalDevice& physical = ctx.physicalDevice();

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = ctx.graphicsQueueFamily();
    const vk::raii::CommandPool command_pool(device, pool_info);

    std::vector<vk::AccelerationStructureInstanceKHR> as_instances{};
    as_instances.reserve(instances.size());

    vk::TransformMatrixKHR identity{};
    identity.matrix = std::array<std::array<float, 4>, 3>{
        std::array<float, 4>{ 1.f, 0.f, 0.f, 0.f },
        std::array<float, 4>{ 0.f, 1.f, 0.f, 0.f },
        std::array<float, 4>{ 0.f, 0.f, 1.f, 0.f },
    };

    for (const BlasInstanceInput& in : instances) {
        vk::AccelerationStructureInstanceKHR inst{};
        inst.transform = identity;
        inst.instanceCustomIndex = in.material_index;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags = static_cast<VkGeometryInstanceFlagsKHR>(
            vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
        inst.accelerationStructureReference = in.blas_address;
        as_instances.push_back(inst);
    }

    const vk::DeviceSize inst_buffer_size =
        static_cast<vk::DeviceSize>(sizeof(vk::AccelerationStructureInstanceKHR) * as_instances.size());
    create_buffer(
        physical,
        device,
        inst_buffer_size,
        vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        out.instance_buffer,
        out.instance_memory);

    void* mapped = out.instance_memory.mapMemory(0, inst_buffer_size);
    std::memcpy(mapped, as_instances.data(), static_cast<std::size_t>(inst_buffer_size));
    out.instance_memory.unmapMemory();

    const vk::DeviceAddress instance_addr =
        device.getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = *out.instance_buffer });

    const vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
        .arrayOfPointers = vk::False,
        .data = instance_addr,
    };
    vk::AccelerationStructureGeometryDataKHR geometry_data{};
    geometry_data.instances = instances_data;
    const vk::AccelerationStructureGeometryKHR geometry{
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = geometry_data,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR build_info{
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &geometry,
    };

    const std::uint32_t primitive_count = static_cast<std::uint32_t>(instances.size());
    const vk::AccelerationStructureBuildSizesInfoKHR sizes =
        device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_info,
            { primitive_count });

    create_buffer(
        physical,
        device,
        sizes.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        out.scratch_buffer,
        out.scratch_memory);
    build_info.scratchData.deviceAddress =
        device.getBufferAddress(vk::BufferDeviceAddressInfo{ .buffer = *out.scratch_buffer });

    create_buffer(
        physical,
        device,
        sizes.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        out.tlas_buffer,
        out.tlas_memory);

    out.tlas = device.createAccelerationStructureKHR(vk::AccelerationStructureCreateInfoKHR{
        .buffer = *out.tlas_buffer,
        .offset = 0,
        .size = sizes.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    });
    build_info.dstAccelerationStructure = *out.tlas;

    vk::AccelerationStructureBuildRangeInfoKHR range_info{
        .primitiveCount = primitive_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    auto cmd = begin_single_time_commands(device, command_pool);
    cmd->buildAccelerationStructuresKHR({ build_info }, { &range_info });
    end_single_time_commands(ctx.graphicsQueue(), *cmd);

    return out;
}

} // namespace renderer::ray_tracing
