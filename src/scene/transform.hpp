#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace scene {

struct Transform {
    glm::mat4 matrix{ 1.0f };

    void translate(float x, float y, float z)
    {
        matrix = glm::translate(matrix, glm::vec3(x, y, z));
    }

    void scale(float factor) { matrix = glm::scale(matrix, glm::vec3(factor)); }

    void rotate_y(float angle_rad)
    {
        matrix = glm::rotate(matrix, angle_rad, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    [[nodiscard]] static float deg_to_rad(float deg)
    {
        static constexpr float kPi = 3.14159265358979323846f;
        return deg * (kPi / 180.0f);
    }
};

} // namespace scene
