#pragma once

#include "scene/mesh_primitive.hpp"

#include <vector>

namespace scene {

struct Mesh {
    std::vector<MeshPrimitive> primitives;
};

} // namespace scene
