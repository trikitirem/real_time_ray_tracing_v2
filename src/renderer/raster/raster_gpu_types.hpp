#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace renderer::raster {

struct CameraUbo {
    glm::mat4 view_proj{ 1.0f };
    glm::vec3 view_position{ 0.0f };
    std::uint32_t _pad0 = 0;
};

struct ModelPushConstant {
    glm::mat4 model{ 1.0f };
    glm::vec4 albedo{ 1.0f };
    std::uint32_t has_texture = 0;
    std::uint32_t _pad0 = 0;
    std::uint32_t _pad1 = 0;
    std::uint32_t _pad2 = 0;
};

struct ShadowPushConstant {
    glm::mat4 light_space_matrix{ 1.0f };
    glm::mat4 model{ 1.0f };
};

struct LightUbo {
    glm::mat4 light_space_matrix{ 1.0f };
    glm::vec3 light_dir_to_light{ 0.0f, 1.0f, 1.0f };
    float _pad0 = 0.0f;
};

} // namespace renderer::raster
