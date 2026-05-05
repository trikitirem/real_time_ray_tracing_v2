#include "renderer/shared/buffers/host_visible_buffer.hpp"

#include <cstring>
#include <stdexcept>

namespace renderer::buffers {

void HostVisibleBuffer::validate_kind(const BufferKind kind)
{
    if (kind != BufferKind::uniform && kind != BufferKind::storage) {
        throw std::runtime_error(
            "HostVisibleBuffer supports only uniform and storage kinds");
    }
}

HostVisibleBuffer::HostVisibleBuffer(const vk::raii::PhysicalDevice& physical_device,
                                     const vk::raii::Device&         device,
                                     const BufferKind                kind,
                                     const vk::DeviceSize            size_bytes)
    : BaseBuffer(physical_device, device, kind)
{
    validate_kind(kind);
    create_with_memory(
        size_bytes, host_visible_usage_for(kind),
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        false);
}

HostVisibleBuffer::HostVisibleBuffer(const vk::raii::PhysicalDevice& physical_device,
                                     const vk::raii::Device&         device,
                                     const BufferKind                kind,
                                     const std::span<const std::byte> initial_data)
    : HostVisibleBuffer(physical_device, device, kind,
                        static_cast<vk::DeviceSize>(initial_data.size_bytes()))
{
    if (initial_data.empty()) {
        throw std::runtime_error("HostVisibleBuffer: initial_data cannot be empty");
    }
    write_bytes(initial_data);
}

void HostVisibleBuffer::write_bytes(const std::span<const std::byte> data,
                                    const vk::DeviceSize             dst_offset_bytes)
{
    const vk::DeviceSize write_size = static_cast<vk::DeviceSize>(data.size_bytes());
    if (dst_offset_bytes + write_size > size_bytes_) {
        throw std::runtime_error("HostVisibleBuffer::write_bytes out of bounds");
    }

    void* mapped = memory_.mapMemory(dst_offset_bytes, write_size);
    std::memcpy(mapped, data.data(), data.size_bytes());
    memory_.unmapMemory();
}

} // namespace renderer::buffers
