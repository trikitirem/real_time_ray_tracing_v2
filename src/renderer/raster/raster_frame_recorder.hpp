#pragma once

#include <vector>

#include "renderer/shared/render_frame_recorder.hpp"
#include "renderer/raster/raster_scene_gpu_data.hpp"
#include "renderer/raster/raster_gpu_types.hpp"

namespace renderer::descriptors {
class UniformSet;
}

namespace renderer::raster {

class RasterPipeline;

class RasterFrameRecorder final : public FrameRecorder {
public:
    explicit RasterFrameRecorder(RasterPipeline& pipeline);
    void set_scene_data(const RasterSceneGpuData* scene_data) { scene_data_ = scene_data; }
    void set_camera_uniform_set(const descriptors::UniformSet* camera_uniform_set)
    {
        camera_uniform_set_ = camera_uniform_set;
    }
    void set_default_texture_uniform_set(const descriptors::UniformSet* default_texture_uniform_set)
    {
        default_texture_uniform_set_ = default_texture_uniform_set;
    }
    void set_per_texture_uniform_sets(const std::vector<descriptors::UniformSet>* per_texture_uniform_sets)
    {
        per_texture_uniform_sets_ = per_texture_uniform_sets;
    }
    void set_light_uniform_set(const descriptors::UniformSet* light_uniform_set)
    {
        light_uniform_set_ = light_uniform_set;
    }
    void set_shadow_push_constant(const ShadowPushConstant& shadow_push)
    {
        shadow_push_ = shadow_push;
    }

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RasterPipeline& pipeline_;
    const RasterSceneGpuData* scene_data_ = nullptr;
    const descriptors::UniformSet* camera_uniform_set_ = nullptr;
    const descriptors::UniformSet* default_texture_uniform_set_ = nullptr;
    const std::vector<descriptors::UniformSet>* per_texture_uniform_sets_ = nullptr;
    const descriptors::UniformSet* light_uniform_set_ = nullptr;
    ShadowPushConstant shadow_push_{};
};

} // namespace renderer::raster
