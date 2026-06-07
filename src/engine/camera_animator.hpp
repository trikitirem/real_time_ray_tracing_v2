#pragma once

#include "engine/camera.hpp"
#include "scene/scene_config.hpp"

namespace engine {

class CameraAnimator {
public:
    void start(const scene::BenchmarkPath& path);
    void stop();
    void update(Camera& camera, float dt);
    [[nodiscard]] bool is_running() const { return running_; }

private:
    scene::BenchmarkPath path_{};
    float                elapsed_s_ = 0.0f;
    bool                 running_   = false;
};

} // namespace engine
