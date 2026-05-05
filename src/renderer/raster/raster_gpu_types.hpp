#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace renderer::raster {

struct CameraUbo {
    glm::mat4 view_proj{ 1.0f };
};

struct ModelPushConstant {
    glm::mat4 model{ 1.0f };
    glm::vec4 albedo{ 1.0f };
};

} // namespace renderer::raster
