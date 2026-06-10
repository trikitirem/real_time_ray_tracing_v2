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

    static constexpr std::uint32_t kShadowMapSize = 2048;

    [[nodiscard]] const vk::raii::RenderPass&  shadow_render_pass() const { return shadow_render_pass_; }
    [[nodiscard]] const vk::raii::Framebuffer& shadow_framebuffer() const { return shadow_framebuffer_; }
    [[nodiscard]] vk::ImageView shadow_depth_view_handle() const { return *shadow_depth_view_; }
    [[nodiscard]] vk::Sampler   shadow_sampler_handle() const { return *shadow_sampler_; }
    [[nodiscard]] vk::Extent2D  shadow_extent() const { return vk::Extent2D{ kShadowMapSize, kShadowMapSize }; }
    [[nodiscard]] const vk::raii::Pipeline&       shadow_pipeline() const { return shadow_pipeline_; }
    [[nodiscard]] const vk::raii::PipelineLayout& shadow_pipeline_layout() const { return shadow_pipeline_layout_; }

    [[nodiscard]] const vk::raii::Pipeline& pipeline_instanced() const
    {
        return pipeline_instanced_;
    }
    [[nodiscard]] const vk::raii::PipelineLayout& pipeline_instanced_layout() const
    {
        return pipeline_instanced_layout_;
    }
    [[nodiscard]] const vk::raii::Pipeline& shadow_pipeline_instanced() const
    {
        return shadow_pipeline_instanced_;
    }
    [[nodiscard]] const vk::raii::PipelineLayout& shadow_pipeline_instanced_layout() const
    {
        return shadow_pipeline_instanced_layout_;
    }

private:
    void create_depth_resources(const vk::raii::Device&         device,
                                const vk::raii::PhysicalDevice& physical,
                                vk::Extent2D                   extent);

    void create_render_pass(const vk::raii::Device& device, vk::Format swapchain_color_format);

    void create_raster_graphics_pipeline(const vk::raii::Device& device);

    void create_framebuffers(const vk::raii::Device& device,
                             const Swapchain&        swapchain,
                             vk::Extent2D            extent);

    void create_shadow_resources(const vk::raii::Device& device,
                                 const vk::raii::PhysicalDevice& physical);
    void create_shadow_render_pass(const vk::raii::Device& device);
    void create_shadow_framebuffer(const vk::raii::Device& device);
    void create_shadow_pipeline(const vk::raii::Device& device,
                                const std::filesystem::path& shadow_spirv_path);

    void create_raster_instanced_pipeline(const vk::raii::Device& device);
    void create_shadow_instanced_pipeline(const vk::raii::Device& device);

    vk::Format depth_format_ = vk::Format::eUndefined;

    vk::raii::RenderPass               render_pass_    = nullptr;
    std::vector<vk::raii::Framebuffer> framebuffers_{};

    vk::raii::DeviceMemory depth_memory_ = nullptr;
    vk::raii::Image        depth_image_  = nullptr;
    vk::raii::ImageView    depth_view_   = nullptr;
    vk::raii::DescriptorSetLayout camera_set_layout_ = nullptr;
    vk::raii::DescriptorSetLayout texture_set_layout_ = nullptr;

    vk::raii::RenderPass shadow_render_pass_ = nullptr;
    vk::raii::Framebuffer shadow_framebuffer_ = nullptr;
    vk::raii::DeviceMemory shadow_depth_memory_ = nullptr;
    vk::raii::Image shadow_depth_image_ = nullptr;
    vk::raii::ImageView shadow_depth_view_ = nullptr;
    vk::raii::Sampler shadow_sampler_ = nullptr;
    vk::raii::DescriptorSetLayout light_set_layout_ = nullptr;
    vk::raii::ShaderModule shadow_shader_module_ = nullptr;
    vk::raii::PipelineLayout shadow_pipeline_layout_ = nullptr;
    vk::raii::Pipeline shadow_pipeline_ = nullptr;

    vk::raii::PipelineLayout pipeline_instanced_layout_        = nullptr;
    vk::raii::Pipeline       pipeline_instanced_               = nullptr;
    vk::raii::PipelineLayout shadow_pipeline_instanced_layout_ = nullptr;
    vk::raii::Pipeline       shadow_pipeline_instanced_        = nullptr;
};

} // namespace renderer::raster
