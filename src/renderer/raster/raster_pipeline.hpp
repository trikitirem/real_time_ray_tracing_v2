#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/shared/graphics_pipeline.hpp"

namespace renderer {

class DeviceContext;
class Swapchain;

} // namespace renderer

namespace renderer::raster {

class RasterPipeline : public renderer::GraphicsPipeline {
public:
    RasterPipeline() = default;
    RasterPipeline(const RasterPipeline&)            = delete;
    RasterPipeline& operator=(const RasterPipeline&) = delete;
    RasterPipeline(RasterPipeline&&)                 = delete;
    RasterPipeline& operator=(RasterPipeline&&)      = delete;

    void create(DeviceContext& ctx, const Swapchain& swapchain, const std::filesystem::path& spirv_path);
    void destroy();

    [[nodiscard]] vk::Format depth_format() const { return depth_format_; }

    [[nodiscard]] const vk::raii::RenderPass&               render_pass() const { return render_pass_; }
    [[nodiscard]] const std::vector<vk::raii::Framebuffer>& framebuffers() const { return framebuffers_; }

private:
    void create_depth_resources(const vk::raii::Device&         device,
                                const vk::raii::PhysicalDevice& physical,
                                vk::Extent2D                   extent);

    void create_render_pass(const vk::raii::Device& device, vk::Format swapchain_color_format);

    void create_raster_graphics_pipeline(const vk::raii::Device& device);

    void create_framebuffers(const vk::raii::Device& device,
                             const Swapchain&        swapchain,
                             vk::Extent2D            extent);

    vk::Format depth_format_ = vk::Format::eUndefined;

    vk::raii::RenderPass               render_pass_    = nullptr;
    std::vector<vk::raii::Framebuffer> framebuffers_{};

    vk::raii::DeviceMemory depth_memory_ = nullptr;
    vk::raii::Image        depth_image_  = nullptr;
    vk::raii::ImageView    depth_view_   = nullptr;
    vk::raii::DescriptorSetLayout camera_set_layout_ = nullptr;
    vk::raii::DescriptorSetLayout texture_set_layout_ = nullptr;
};

} // namespace renderer::raster
