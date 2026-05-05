#pragma once

#include "renderer/shared/scene_gpu_builder_base.hpp"

namespace renderer::ray_tracing {

class RtSceneGpuBuilder final : public SceneGpuBuilderBase {
public:
    RtSceneGpuBuilder() = default;
    ~RtSceneGpuBuilder() override = default;

    SceneGpuData build(DeviceContext& ctx, const scene::Scene& scene) override;
};

} // namespace renderer::ray_tracing
