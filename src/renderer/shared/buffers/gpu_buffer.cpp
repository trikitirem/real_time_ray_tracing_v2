#include "renderer/shared/buffers/gpu_buffer.hpp"

#include <cstring>
#include <stdexcept>

namespace renderer::buffers {

GpuBuffer::GpuBuffer(const vk::raii::PhysicalDevice& physical_device,
                     const vk::raii::Device&         device,
                     const vk::raii::CommandPool&    command_pool,
                     const vk::raii::Queue&          graphics_queue,
                     const BufferKind                kind,
                     const std::span<const std::byte> initial_data)
    : BaseBuffer(physical_device, device, kind)
{
    if (initial_data.empty()) {
        throw std::runtime_error("GpuBuffer: initial_data cannot be empty");
    }

    const vk::DeviceSize size_bytes = static_cast<vk::DeviceSize>(initial_data.size_bytes());

    Allocation staging = create_allocation(
        device, mem_props_, size_bytes, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        false);

    void* mapped = staging.memory.mapMemory(0, size_bytes);
    std::memcpy(mapped, initial_data.data(), initial_data.size_bytes());
    staging.memory.unmapMemory();

    create_with_memory(size_bytes, gpu_usage_for(kind),
                       vk::MemoryPropertyFlagBits::eDeviceLocal,
                       requires_device_address(kind));

    copy_buffer_one_shot(device, command_pool, graphics_queue, *staging.buffer, *buffer_,
                         size_bytes);
}

vk::DeviceAddress GpuBuffer::device_address() const
{
    vk::BufferDeviceAddressInfo info{};
    info.buffer = *buffer_;
    return device_->getBufferAddress(info);
}

} // namespace renderer::buffers
