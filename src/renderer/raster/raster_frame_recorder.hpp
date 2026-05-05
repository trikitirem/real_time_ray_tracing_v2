#pragma once

#include "renderer/shared/render_frame_recorder.hpp"
#include "renderer/shared/scene_gpu_data.hpp"

namespace renderer::raster {

class RasterPipeline;

class RasterFrameRecorder final : public FrameRecorder {
public:
    explicit RasterFrameRecorder(RasterPipeline& pipeline);
    void set_scene_data(const SceneGpuData* scene_data) { scene_data_ = scene_data; }

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RasterPipeline& pipeline_;
    const SceneGpuData* scene_data_ = nullptr;
};

} // namespace renderer::raster
