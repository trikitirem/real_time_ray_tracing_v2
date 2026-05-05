#include "renderer/ray_tracing/rt_scene_gpu_builder.hpp"

#include "renderer/shared/device_context.hpp"
#include "scene/scene.hpp"

namespace renderer::ray_tracing {

ScenePayload RtSceneGpuBuilder::build(DeviceContext& ctx, const scene::Scene& scene)
{
    (void)ctx;
    RtSceneGpuData out{};
    out.valid = !scene.models().empty();
    ScenePayload payload{};
    payload.backend = BackendKind::ray_tracing;
    payload.set(std::move(out));
    return payload;
}

} // namespace renderer::ray_tracing
