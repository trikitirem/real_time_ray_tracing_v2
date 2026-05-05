#pragma once

#include <memory>

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/shared/render_backend.hpp"

namespace renderer::raster {

class RasterRenderBackend final : public renderer::IRenderBackend {
public:
    RasterRenderBackend() = default;
    ~RasterRenderBackend() override;

    void create(DeviceContext& ctx, const Swapchain& swapchain) override;
    void destroy(DeviceContext& ctx) override;
    void load_scene(SceneGpuData&& scene_data) override;
    void record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx) override;

private:
    std::unique_ptr<RasterPipeline> pipeline_;
    std::unique_ptr<RasterFrameRecorder> frame_recorder_;
    SceneGpuData scene_data_{};
};

} // namespace renderer::raster
