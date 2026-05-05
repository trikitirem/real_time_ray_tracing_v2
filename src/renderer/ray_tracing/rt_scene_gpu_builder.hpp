#pragma once

#include "renderer/ray_tracing/rt_scene_gpu_data.hpp"
#include "renderer/shared/scene_gpu_builder_base.hpp"

namespace renderer::ray_tracing {

class RtSceneGpuBuilder final : public SceneGpuBuilderBase {
public:
    RtSceneGpuBuilder() = default;
    ~RtSceneGpuBuilder() override = default;

    ScenePayload build(DeviceContext& ctx, const scene::Scene& scene) override;
};

} // namespace renderer::ray_tracing
