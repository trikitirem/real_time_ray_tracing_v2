#pragma once

#include "util/asset_location.hpp"

#include <glm/vec3.hpp>

namespace scene {

struct Material {
    util::AssetLocation texture_location{};
    glm::vec3           base_color{ 1.0f };
    float               metalness  = 0.0f;
    float               roughness  = 0.5f;
};

} // namespace scene
