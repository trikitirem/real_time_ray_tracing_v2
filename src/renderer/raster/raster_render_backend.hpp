#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_gpu_types.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/shared/buffers/host_visible_buffer.hpp"
#include "renderer/shared/descriptors/uniform_set.hpp"
#include "renderer/shared/render_backend.hpp"

namespace renderer::raster {

class RasterRenderBackend final : public renderer::IRenderBackend {
public:
    RasterRenderBackend() = default;
    ~RasterRenderBackend() override;

    void create(DeviceContext& ctx, const Swapchain& swapchain) override;
    void destroy(DeviceContext& ctx) override;
    void load_scene(SceneGpuData&& scene_data) override;
    void update_camera(const scene::Camera& camera, vk::Extent2D extent) override;
    void record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx) override;

private:
    static constexpr std::uint32_t kFramesInFlight = 2;

    std::unique_ptr<RasterPipeline> pipeline_;
    std::unique_ptr<RasterFrameRecorder> frame_recorder_;
    SceneGpuData scene_data_{};
    std::optional<buffers::HostVisibleBuffer> camera_buffer_;
    std::optional<descriptors::UniformSet> camera_uniform_set_;
    std::vector<descriptors::UniformSet> texture_uniform_sets_{};
};

} // namespace renderer::raster
