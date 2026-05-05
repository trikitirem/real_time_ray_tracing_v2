#pragma once

#include "renderer/shared/buffers/base_buffer.hpp"

#include <cstddef>
#include <span>
#include <type_traits>

namespace renderer::buffers {

class GpuBuffer final : public BaseBuffer {
public:
    GpuBuffer(const vk::raii::PhysicalDevice& physical_device,
              const vk::raii::Device&         device,
              const vk::raii::CommandPool&    command_pool,
              const vk::raii::Queue&          graphics_queue,
              BufferKind                      kind,
              std::span<const std::byte>      initial_data);

    template <typename T>
    static GpuBuffer from_span(const vk::raii::PhysicalDevice& physical_device,
                               const vk::raii::Device&         device,
                               const vk::raii::CommandPool&    command_pool,
                               const vk::raii::Queue&          graphics_queue,
                               BufferKind                      kind,
                               const std::span<const T>        data)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "GpuBuffer::from_span requires trivially copyable data");
        return GpuBuffer(physical_device, device, command_pool, graphics_queue, kind,
                         std::as_bytes(data));
    }

    [[nodiscard]] vk::DeviceAddress device_address() const;
};

} // namespace renderer::buffers
