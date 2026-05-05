#pragma once

#include "renderer/shared/render_frame_recorder.hpp"
#include "renderer/shared/scene_gpu_data.hpp"

namespace renderer::descriptors {
class UniformSet;
}

namespace renderer::raster {

class RasterPipeline;

class RasterFrameRecorder final : public FrameRecorder {
public:
    explicit RasterFrameRecorder(RasterPipeline& pipeline);
    void set_scene_data(const SceneGpuData* scene_data) { scene_data_ = scene_data; }
    void set_camera_uniform_set(const descriptors::UniformSet* camera_uniform_set)
    {
        camera_uniform_set_ = camera_uniform_set;
    }
    void set_texture_uniform_set(const descriptors::UniformSet* texture_uniform_set)
    {
        texture_uniform_set_ = texture_uniform_set;
    }

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RasterPipeline& pipeline_;
    const SceneGpuData* scene_data_ = nullptr;
    const descriptors::UniformSet* camera_uniform_set_ = nullptr;
    const descriptors::UniformSet* texture_uniform_set_ = nullptr;
};

} // namespace renderer::raster
