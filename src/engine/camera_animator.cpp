#include "engine/camera_animator.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine {

namespace {

glm::quat look_at_orientation(const glm::vec3& position, const glm::vec3& look_at)
{
    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    const glm::vec3 forward = look_at - position;
    if (glm::length(forward) < 1e-6f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (glm::length(glm::cross(forward, world_up)) < 1e-6f) {
        world_up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 view = glm::lookAt(position, look_at, world_up);
    const glm::mat4 cam_to_world = glm::inverse(view);
    return glm::normalize(glm::quat_cast(cam_to_world));
}

} // namespace

void CameraAnimator::start(const scene::BenchmarkPath& path)
{
    path_     = path;
    elapsed_s_ = 0.0f;
    running_  = true;
}

void CameraAnimator::stop()
{
    running_ = false;
}

void CameraAnimator::update(Camera& camera, const float dt)
{
    if (!running_) {
        return;
    }

    elapsed_s_ += dt;
    const float t = path_.duration_seconds > 0.0f
                        ? std::clamp(elapsed_s_ / path_.duration_seconds, 0.0f, 1.0f)
                        : 1.0f;

    camera.position   = glm::mix(path_.from, path_.to, t);
    camera.orientation = look_at_orientation(camera.position, path_.look_at);

    if (elapsed_s_ >= path_.duration_seconds) {
        running_ = false;
    }
}

} // namespace engine
