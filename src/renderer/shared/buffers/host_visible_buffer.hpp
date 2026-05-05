#pragma once

#include "renderer/shared/buffers/base_buffer.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace renderer::buffers {

class HostVisibleBuffer final : public BaseBuffer {
public:
    HostVisibleBuffer(const vk::raii::PhysicalDevice& physical_device,
                      const vk::raii::Device&         device,
                      BufferKind                      kind,
                      vk::DeviceSize                  size_bytes);

    HostVisibleBuffer(const vk::raii::PhysicalDevice& physical_device,
                      const vk::raii::Device&         device,
                      BufferKind                      kind,
                      std::span<const std::byte>      initial_data);

    void write_bytes(std::span<const std::byte> data, vk::DeviceSize dst_offset_bytes = 0);

    template <typename T>
    void update(const std::span<const T> data)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "HostVisibleBuffer::update requires trivially copyable data");
        const std::span<const std::byte> bytes = std::as_bytes(data);
        if (bytes.size_bytes() != size_bytes()) {
            throw std::runtime_error(
                "HostVisibleBuffer::update strict mode: size mismatch");
        }
        write_bytes(bytes, 0);
    }

    template <typename T>
    static HostVisibleBuffer from_span(const vk::raii::PhysicalDevice& physical_device,
                                       const vk::raii::Device&         device,
                                       BufferKind                      kind,
                                       const std::span<const T>        data)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "HostVisibleBuffer::from_span requires trivially copyable data");
        return HostVisibleBuffer(physical_device, device, kind, std::as_bytes(data));
    }

private:
    static void validate_kind(BufferKind kind);
};

} // namespace renderer::buffers
