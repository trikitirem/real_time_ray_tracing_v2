#pragma once

#include "renderer/ray_tracing/rt_scene_gpu_data.hpp"
#include "renderer/shared/render_frame_recorder.hpp"

namespace renderer::descriptors {
class UniformSet;
}

namespace renderer {

class Swapchain;

} // namespace renderer

namespace renderer::ray_tracing {

class RayTracingPipeline;

class RayTracingFrameRecorder final : public FrameRecorder {
public:
    RayTracingFrameRecorder(RayTracingPipeline& pipeline,
                            const renderer::Swapchain& swapchain,
                            vk::Format depthFormat,
                            vk::ImageView depthImageView);
    void set_camera_uniform_set(const descriptors::UniformSet* camera_uniform_set)
    {
        camera_uniform_set_ = camera_uniform_set;
    }
    void set_texture_uniform_set(const descriptors::UniformSet* texture_uniform_set)
    {
        texture_uniform_set_ = texture_uniform_set;
    }
    void set_scene_data(const RtSceneGpuData* scene_data)
    {
        scene_data_ = scene_data;
    }

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RayTracingPipeline&       pipeline_;
    const renderer::Swapchain& swapchain_;
    vk::Format               depth_format_;
    vk::ImageView            depth_view_;
    const RtSceneGpuData* scene_data_ = nullptr;
    const descriptors::UniformSet* camera_uniform_set_ = nullptr;
    const descriptors::UniformSet* texture_uniform_set_ = nullptr;
};

} // namespace renderer::ray_tracing
