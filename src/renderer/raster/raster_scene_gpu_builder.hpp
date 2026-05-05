#pragma once

#include "renderer/raster/raster_scene_gpu_data.hpp"
#include "renderer/shared/scene_gpu_builder_base.hpp"

namespace renderer::raster {

class RasterSceneGpuBuilder final : public SceneGpuBuilderBase {
public:
    RasterSceneGpuBuilder() = default;
    ~RasterSceneGpuBuilder() override = default;

    ScenePayload build(DeviceContext& ctx, const scene::Scene& scene) override;
};

} // namespace renderer::raster
