#pragma once

#include "renderer/shared/scene_gpu_builder_base.hpp"

namespace renderer::raster {

class RasterSceneGpuBuilder final : public SceneGpuBuilderBase {
public:
    RasterSceneGpuBuilder() = default;
    ~RasterSceneGpuBuilder() override = default;

    SceneGpuData build(DeviceContext& ctx, const scene::Scene& scene) override;
};

} // namespace renderer::raster
