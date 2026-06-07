#pragma once

#include "util/asset_location.hpp"

#include <cstdint>
#include <filesystem>

#include <vulkan/vulkan_raii.hpp>

namespace renderer::textures {

class TextureResource {
public:
    TextureResource() = default;
    TextureResource(const TextureResource&)            = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    TextureResource(TextureResource&&)                 = default;
    TextureResource& operator=(TextureResource&&)      = default;
    ~TextureResource()                                 = default;

    static TextureResource load_from_asset_location(
        const vk::raii::PhysicalDevice& physical_device,
        const vk::raii::Device&         device,
        const vk::raii::CommandPool&    command_pool,
        const vk::raii::Queue&          graphics_queue,
        const util::AssetLocation&      location);

    static TextureResource create_solid_rgba(
        const vk::raii::PhysicalDevice& physical_device,
        const vk::raii::Device&         device,
        const vk::raii::CommandPool&    command_pool,
        const vk::raii::Queue&          graphics_queue,
        std::uint8_t                    r,
        std::uint8_t                    g,
        std::uint8_t                    b,
        std::uint8_t                    a = 255);

    [[nodiscard]] const vk::raii::Image&       image() const { return image_; }
    [[nodiscard]] const vk::raii::ImageView&   view() const { return view_; }
    [[nodiscard]] const vk::raii::Sampler&     sampler() const { return sampler_; }
    [[nodiscard]] vk::Format                   format() const { return format_; }
    [[nodiscard]] vk::Extent2D                 extent() const { return extent_; }
    [[nodiscard]] vk::ImageLayout              shader_layout() const
    {
        return vk::ImageLayout::eShaderReadOnlyOptimal;
    }

private:
    static std::uint32_t find_memory_type(const vk::PhysicalDeviceMemoryProperties& mem_props,
                                          std::uint32_t type_bits,
                                          vk::MemoryPropertyFlags required_properties);

    static void transition_image_layout_one_shot(const vk::raii::Device&      device,
                                                 const vk::raii::CommandPool& command_pool,
                                                 const vk::raii::Queue&       queue,
                                                 vk::Image                    image,
                                                 vk::ImageLayout              old_layout,
                                                 vk::ImageLayout              new_layout);

    static void copy_buffer_to_image_one_shot(const vk::raii::Device&      device,
                                              const vk::raii::CommandPool& command_pool,
                                              const vk::raii::Queue&       queue,
                                              vk::Buffer                   src_buffer,
                                              vk::Image                    dst_image,
                                              vk::Extent2D                 extent);

    vk::raii::Image        image_   = nullptr;
    vk::raii::DeviceMemory memory_  = nullptr;
    vk::raii::ImageView    view_    = nullptr;
    vk::raii::Sampler      sampler_ = nullptr;

    vk::Format   format_ = vk::Format::eR8G8B8A8Srgb;
    vk::Extent2D extent_{};
};

} // namespace renderer::textures
