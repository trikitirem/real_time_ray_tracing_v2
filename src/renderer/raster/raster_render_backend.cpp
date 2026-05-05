#include "renderer/raster/raster_render_backend.hpp"

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/swapchain.hpp"
#include "util/shader_paths.hpp"

namespace renderer::raster {

RasterRenderBackend::~RasterRenderBackend() = default;

void RasterRenderBackend::create(DeviceContext& ctx, const Swapchain& swapchain)
{
    pipeline_ = std::make_unique<RasterPipeline>();
    pipeline_->create(ctx, swapchain, util::raster_shader("default.spv").full_path());
    frame_recorder_ = std::make_unique<RasterFrameRecorder>(*pipeline_);
}

void RasterRenderBackend::destroy(DeviceContext& ctx)
{
    (void)ctx;
    frame_recorder_.reset();
    if (pipeline_) {
        pipeline_->destroy();
        pipeline_.reset();
    }
}

void RasterRenderBackend::record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx)
{
    frame_recorder_->record(cmd, frame_ctx);
}

} // namespace renderer::raster
