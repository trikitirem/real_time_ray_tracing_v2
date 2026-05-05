#pragma once

#include "scene/mesh.hpp"
#include "scene/transform.hpp"

namespace scene {

struct Model {
    Mesh      mesh;
    Transform transform{};
};

} // namespace scene
