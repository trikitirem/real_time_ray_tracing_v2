#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

namespace scene {

struct Transform {
    glm::mat4 matrix{ 1.0f };

    void translate(float x, float y, float z)
    {
        matrix = glm::translate(matrix, glm::vec3(x, y, z));
    }

    void scale(float factor) { matrix = glm::scale(matrix, glm::vec3(factor)); }

    void scale(float x, float y, float z)
    {
        matrix = glm::scale(matrix, glm::vec3(x, y, z));
    }

    void rotate_y(float angle_rad)
    {
        matrix = glm::rotate(matrix, angle_rad, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    /// Matches camera euler convention: yaw(Y) * pitch(X) * roll(Z), degrees.
    void apply_euler_degrees(const glm::vec3& euler_deg)
    {
        const glm::vec3              rad       = glm::radians(euler_deg);
        const glm::quat                yaw_rot   = glm::angleAxis(rad.y, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat                pitch_rot = glm::angleAxis(rad.x, glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat                roll_rot  = glm::angleAxis(rad.z, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat                rot       = glm::normalize(yaw_rot * pitch_rot * roll_rot);
        matrix                                   = matrix * glm::mat4_cast(rot);
    }

    [[nodiscard]] static float deg_to_rad(float deg)
    {
        static constexpr float kPi = 3.14159265358979323846f;
        return deg * (kPi / 180.0f);
    }
};

} // namespace scene
