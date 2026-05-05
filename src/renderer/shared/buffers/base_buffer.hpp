#pragma once

#include "renderer/shared/buffers/buffer_kind.hpp"

#include <cstdint>

#include <vulkan/vulkan_raii.hpp>

namespace renderer::buffers {

class BaseBuffer {
public:
    BaseBuffer(const BaseBuffer&)            = delete;
    BaseBuffer& operator=(const BaseBuffer&) = delete;
    BaseBuffer(BaseBuffer&&)                 = default;
    BaseBuffer& operator=(BaseBuffer&&)      = default;
    virtual ~BaseBuffer()                    = default;

    [[nodiscard]] const vk::raii::Buffer& buffer() const { return buffer_; }
    [[nodiscard]] vk::DeviceSize           size_bytes() const { return size_bytes_; }
    [[nodiscard]] BufferKind               kind() const { return kind_; }

protected:
    struct Allocation {
        vk::raii::Buffer       buffer = nullptr;
        vk::raii::DeviceMemory memory = nullptr;
    };

    BaseBuffer(const vk::raii::PhysicalDevice& physical_device,
               const vk::raii::Device&         device,
               BufferKind                      kind);

    void create_with_memory(vk::DeviceSize         size_bytes,
                            vk::BufferUsageFlags   usage,
                            vk::MemoryPropertyFlags memory_properties,
                            bool                   require_device_address);

    [[nodiscard]] static Allocation create_allocation(
        const vk::raii::Device&                device,
        const vk::PhysicalDeviceMemoryProperties& mem_props,
        vk::DeviceSize                         size_bytes,
        vk::BufferUsageFlags                   usage,
        vk::MemoryPropertyFlags                memory_properties,
        bool                                   require_device_address);

    [[nodiscard]] static std::uint32_t find_memory_type(
        const vk::PhysicalDeviceMemoryProperties& mem_props,
        std::uint32_t                          type_bits,
        vk::MemoryPropertyFlags                required_properties);

    [[nodiscard]] static vk::BufferUsageFlags gpu_usage_for(BufferKind kind);
    [[nodiscard]] static vk::BufferUsageFlags host_visible_usage_for(BufferKind kind);
    [[nodiscard]] static bool                 requires_device_address(BufferKind kind);

    static void copy_buffer_one_shot(const vk::raii::Device&      device,
                                     const vk::raii::CommandPool& command_pool,
                                     const vk::raii::Queue&       queue,
                                     vk::Buffer                   src,
                                     vk::Buffer                   dst,
                                     vk::DeviceSize               size_bytes);

    vk::PhysicalDeviceMemoryProperties mem_props_{};
    const vk::raii::Device*            device_ = nullptr;
    BufferKind                         kind_;
    vk::DeviceSize                     size_bytes_ = 0;

    vk::raii::Buffer       buffer_ = nullptr;
    vk::raii::DeviceMemory memory_ = nullptr;
};

} // namespace renderer::buffers
