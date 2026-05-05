#pragma once

#include "renderer/shared/render_frame_recorder.hpp"

namespace renderer {

class Swapchain;

} // namespace renderer

namespace renderer::ray_tracing {

class RayTracingPipeline;

class RayTracingFrameRecorder final : public RenderFrameRecorder {
public:
    RayTracingFrameRecorder(RayTracingPipeline& pipeline,
                            const renderer::Swapchain& swapchain,
                            vk::Format depthFormat,
                            vk::ImageView depthImageView);

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RayTracingPipeline&       pipeline_;
    const renderer::Swapchain& swapchain_;
    vk::Format               depth_format_;
    vk::ImageView            depth_view_;
};

} // namespace renderer::ray_tracing
