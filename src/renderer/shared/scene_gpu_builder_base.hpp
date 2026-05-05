#pragma once

#include "renderer/shared/scene_gpu_data.hpp"

namespace scene {
class Scene;
}

namespace renderer {

class DeviceContext;

class SceneGpuBuilderBase {
public:
    SceneGpuBuilderBase() = default;
    SceneGpuBuilderBase(const SceneGpuBuilderBase&) = delete;
    SceneGpuBuilderBase& operator=(const SceneGpuBuilderBase&) = delete;
    virtual ~SceneGpuBuilderBase() = default;

    virtual SceneGpuData build(DeviceContext& ctx, const scene::Scene& scene) = 0;
};

} // namespace renderer
