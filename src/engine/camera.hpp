#pragma once

#include "engine/delta_time.hpp"
#include "engine/input_controller.hpp"

#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {

/// World space is right-handed, Y up (matches `make_viewport_y_up_ndc`: no extra Y flip here).
class Camera {
public:
    glm::vec3 position{ 0.0f };
    glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f }; // w, x, y, z — identity looks along world −Z

    float fov_y_rad = 1.0471975511965977461542144610932f; // ~60°
    float z_near    = 0.1f;
    float z_far     = 100.0f;
    float move_speed = 4.5f;
    float mouse_sensitivity = 0.0025f;

    [[nodiscard]] glm::mat4 view_matrix() const
    {
        const glm::mat4 cam_to_world
            = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(orientation);
        return glm::inverse(cam_to_world);
    }

    [[nodiscard]] glm::mat4 projection_matrix(float aspect) const
    {
        return glm::perspective(fov_y_rad, aspect, z_near, z_far);
    }

    [[nodiscard]] glm::mat4 view_projection(float aspect) const
    {
        return projection_matrix(aspect) * view_matrix();
    }

    void update_from_input(const InputController& input)
    {
        const float dt = DeltaTime::instance().seconds();
        if (dt <= 0.0f) {
            return;
        }

        const glm::vec3 forward = glm::normalize(orientation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 right = glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 world_up(0.0f, 1.0f, 0.0f);

        glm::vec3 move_dir(0.0f);
        if (input.move_forward()) {
            move_dir += forward;
        }
        if (input.move_backward()) {
            move_dir -= forward;
        }
        if (input.move_right()) {
            move_dir += right;
        }
        if (input.move_left()) {
            move_dir -= right;
        }
        if (input.move_up()) {
            move_dir += world_up;
        }
        if (input.move_down()) {
            move_dir -= world_up;
        }
        if (glm::length(move_dir) > 0.0f) {
            position += glm::normalize(move_dir) * (move_speed * dt);
        }

        const float yaw_delta = -input.mouse_delta_x() * mouse_sensitivity;
        const float pitch_delta_raw = -input.mouse_delta_y() * mouse_sensitivity;
        const float max_pitch = glm::radians(89.0f);
        const float pitch_delta = std::clamp(
            pitch_delta_raw,
            -max_pitch - pitch_accumulated_rad_,
            max_pitch - pitch_accumulated_rad_);
        pitch_accumulated_rad_ += pitch_delta;

        if (yaw_delta != 0.0f) {
            const glm::quat yaw_rot = glm::angleAxis(yaw_delta, world_up);
            orientation = glm::normalize(yaw_rot * orientation);
        }
        if (pitch_delta != 0.0f) {
            const glm::vec3 local_right = glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::quat pitch_rot = glm::angleAxis(pitch_delta, local_right);
            orientation = glm::normalize(pitch_rot * orientation);
        }
    }

private:
    float pitch_accumulated_rad_ = 0.0f;
};

} // namespace engine
