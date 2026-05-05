#include "renderer/shared/buffers/base_buffer.hpp"

#include <stdexcept>

namespace renderer::buffers {

BaseBuffer::BaseBuffer(const vk::raii::PhysicalDevice& physical_device,
                       const vk::raii::Device&         device,
                       const BufferKind                kind)
    : mem_props_(physical_device.getMemoryProperties())
    , device_(&device)
    , kind_(kind)
{
}

void BaseBuffer::create_with_memory(const vk::DeviceSize         size_bytes,
                                    const vk::BufferUsageFlags   usage,
                                    const vk::MemoryPropertyFlags memory_properties,
                                    const bool                   require_device_address)
{
    Allocation alloc = create_allocation(
        *device_, mem_props_, size_bytes, usage, memory_properties, require_device_address);
    buffer_     = std::move(alloc.buffer);
    memory_     = std::move(alloc.memory);
    size_bytes_ = size_bytes;
}

BaseBuffer::Allocation BaseBuffer::create_allocation(
    const vk::raii::Device&                device,
    const vk::PhysicalDeviceMemoryProperties& mem_props,
    const vk::DeviceSize                   size_bytes,
    const vk::BufferUsageFlags             usage,
    const vk::MemoryPropertyFlags          memory_properties,
    const bool                             require_device_address)
{
    vk::BufferCreateInfo buffer_ci{};
    buffer_ci.size        = size_bytes;
    buffer_ci.usage       = usage;
    buffer_ci.sharingMode = vk::SharingMode::eExclusive;

    Allocation alloc{};
    alloc.buffer = vk::raii::Buffer(device, buffer_ci);

    const vk::MemoryRequirements req = alloc.buffer.getMemoryRequirements();
    const std::uint32_t          memory_type_index
        = find_memory_type(mem_props, req.memoryTypeBits, memory_properties);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize  = req.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    vk::MemoryAllocateFlagsInfo flags_info{};
    if (require_device_address) {
        flags_info.flags  = vk::MemoryAllocateFlagBits::eDeviceAddress;
        alloc_info.pNext = &flags_info;
    }

    alloc.memory = vk::raii::DeviceMemory(device, alloc_info);
    alloc.buffer.bindMemory(*alloc.memory, 0);
    return alloc;
}

std::uint32_t BaseBuffer::find_memory_type(
    const vk::PhysicalDeviceMemoryProperties& mem_props,
    const std::uint32_t                       type_bits,
    const vk::MemoryPropertyFlags             required_properties)
{
    for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        const bool supported = (type_bits & (1u << i)) != 0;
        const bool has_props = (mem_props.memoryTypes[i].propertyFlags & required_properties)
                            == required_properties;
        if (supported && has_props) {
            return i;
        }
    }
    throw std::runtime_error("BaseBuffer::find_memory_type: no suitable memory type");
}

vk::BufferUsageFlags BaseBuffer::gpu_usage_for(const BufferKind kind)
{
    switch (kind) {
    case BufferKind::vertex:
        return vk::BufferUsageFlagBits::eVertexBuffer
             | vk::BufferUsageFlagBits::eTransferDst
             | vk::BufferUsageFlagBits::eShaderDeviceAddress
             | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
             | vk::BufferUsageFlagBits::eStorageBuffer;
    case BufferKind::index:
        return vk::BufferUsageFlagBits::eIndexBuffer
             | vk::BufferUsageFlagBits::eTransferDst
             | vk::BufferUsageFlagBits::eShaderDeviceAddress
             | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
             | vk::BufferUsageFlagBits::eStorageBuffer;
    case BufferKind::uniform:
        return vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
    case BufferKind::storage:
        return vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
             | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }
    throw std::runtime_error("BaseBuffer::gpu_usage_for: unsupported BufferKind");
}

vk::BufferUsageFlags BaseBuffer::host_visible_usage_for(const BufferKind kind)
{
    switch (kind) {
    case BufferKind::uniform:
        return vk::BufferUsageFlagBits::eUniformBuffer;
    case BufferKind::storage:
        return vk::BufferUsageFlagBits::eStorageBuffer;
    case BufferKind::vertex:
    case BufferKind::index:
        break;
    }
    throw std::runtime_error(
        "BaseBuffer::host_visible_usage_for: host-visible buffer supports only uniform/storage");
}

bool BaseBuffer::requires_device_address(const BufferKind kind)
{
    switch (kind) {
    case BufferKind::vertex:
    case BufferKind::index:
    case BufferKind::storage:
        return true;
    case BufferKind::uniform:
        return false;
    }
    throw std::runtime_error("BaseBuffer::requires_device_address: unsupported BufferKind");
}

void BaseBuffer::copy_buffer_one_shot(const vk::raii::Device&      device,
                                      const vk::raii::CommandPool& command_pool,
                                      const vk::raii::Queue&       queue,
                                      const vk::Buffer             src,
                                      const vk::Buffer             dst,
                                      const vk::DeviceSize         size_bytes)
{
    vk::CommandBufferAllocateInfo alloc{};
    alloc.commandPool        = *command_pool;
    alloc.level              = vk::CommandBufferLevel::ePrimary;
    alloc.commandBufferCount = 1;

    vk::raii::CommandBuffers cmd_buffers = vk::raii::CommandBuffers(device, alloc);
    vk::raii::CommandBuffer& cmd         = cmd_buffers.front();

    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    cmd.copyBuffer(src, dst, vk::BufferCopy{
                                 .srcOffset = 0,
                                 .dstOffset = 0,
                                 .size      = size_bytes,
                             });
    cmd.end();

    const vk::CommandBuffer cmd_raw = *cmd;
    queue.submit(vk::SubmitInfo{
                     .commandBufferCount = 1,
                     .pCommandBuffers    = &cmd_raw,
                 },
                 nullptr);
    queue.waitIdle();
}

} // namespace renderer::buffers
