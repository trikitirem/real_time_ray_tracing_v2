#pragma once

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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
};

} // namespace engine
