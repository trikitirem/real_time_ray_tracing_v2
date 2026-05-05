#pragma once

#include "renderer/shared/render_frame_recorder.hpp"

namespace renderer::raster {

class RasterPipeline;

class RasterFrameRecorder final : public FrameRecorder {
public:
    explicit RasterFrameRecorder(RasterPipeline& pipeline);

    void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) override;

private:
    RasterPipeline& pipeline_;
};

} // namespace renderer::raster
