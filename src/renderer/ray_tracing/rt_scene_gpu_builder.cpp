#include "renderer/ray_tracing/rt_scene_gpu_builder.hpp"

#include "renderer/shared/device_context.hpp"
#include "scene/scene.hpp"

namespace renderer::ray_tracing {

SceneGpuData RtSceneGpuBuilder::build(DeviceContext& ctx, const scene::Scene& scene)
{
    (void)ctx;
    SceneGpuData out{};
    out.valid = !scene.models().empty();
    return out;
}

} // namespace renderer::ray_tracing
