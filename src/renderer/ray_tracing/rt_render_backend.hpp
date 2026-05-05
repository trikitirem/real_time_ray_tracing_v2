#pragma once

#include <memory>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/ray_tracing/rt_frame_recorder.hpp"
#include "renderer/ray_tracing/rt_pipeline.hpp"
#include "renderer/ray_tracing/rt_scene_gpu_data.hpp"
#include "renderer/shared/render_backend.hpp"

namespace renderer::ray_tracing {

class RtRenderBackend final : public renderer::IRenderBackend {
public:
    RtRenderBackend() = default;
    ~RtRenderBackend() override;

    void create(DeviceContext& ctx, const Swapchain& swapchain) override;
    void destroy(DeviceContext& ctx) override;
    void load_scene(ScenePayload&& scene_payload) override;
    void update_camera(const scene::Camera& camera, vk::Extent2D extent) override;
    void record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx) override;

private:
    void create_depth_resources(DeviceContext& ctx, vk::Extent2D extent);
    void destroy_depth_resources();

    std::unique_ptr<RayTracingPipeline> pipeline_;
    std::unique_ptr<RayTracingFrameRecorder> frame_recorder_;

    vk::Format rt_depth_format_ = vk::Format::eUndefined;
    vk::raii::Image rt_depth_image_ = nullptr;
    vk::raii::DeviceMemory rt_depth_memory_ = nullptr;
    vk::raii::ImageView rt_depth_view_ = nullptr;
    RtSceneGpuData scene_data_{};
};

} // namespace renderer::ray_tracing
