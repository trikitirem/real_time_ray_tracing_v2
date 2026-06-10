#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_gpu_types.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/raster/raster_scene_gpu_data.hpp"
#include "renderer/shared/buffers/host_visible_buffer.hpp"
#include "renderer/shared/descriptors/uniform_set.hpp"
#include "renderer/shared/render_backend.hpp"
#include "renderer/shared/textures/texture_resource.hpp"

namespace renderer::raster {

class RasterRenderBackend final : public renderer::IRenderBackend {
public:
    RasterRenderBackend() = default;
    ~RasterRenderBackend() override;

    void create(DeviceContext& ctx, const Swapchain& swapchain) override;
    void destroy(DeviceContext& ctx) override;
    void load_scene(ScenePayload&& scene_payload) override;
    void update_camera(const engine::Camera& camera, vk::Extent2D extent) override;
    void update_light(vk::Extent2D extent) override;
    void set_shadow_half_extent(float half_extent);

    void record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx) override;

private:
    void build_texture_descriptor_sets();

    DeviceContext* ctx_ = nullptr;
    std::unique_ptr<RasterPipeline> pipeline_;
    std::unique_ptr<RasterFrameRecorder> frame_recorder_;
    RasterSceneGpuData scene_data_{};
    std::optional<buffers::HostVisibleBuffer> camera_buffer_;
    std::optional<descriptors::UniformSet> camera_uniform_set_;
    std::optional<descriptors::UniformSet> default_texture_uniform_set_;
    std::vector<descriptors::UniformSet> per_texture_uniform_sets_{};
    std::optional<buffers::HostVisibleBuffer> light_buffer_;
    std::optional<descriptors::UniformSet> light_uniform_set_;
    textures::TextureResource default_albedo_{};

    float shadow_half_extent_ = 10.0f;
};

} // namespace renderer::raster
