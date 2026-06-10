#pragma once

#include "scene/model.hpp"

#include <glm/mat4x4.hpp>
#include <vector>

namespace scene {

struct InstancedGroup {
    Model prototype;
    std::vector<glm::mat4> transforms;
};

} // namespace scene
