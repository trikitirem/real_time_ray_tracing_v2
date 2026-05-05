#pragma once

#include "scene/material.hpp"
#include "util/vertex.hpp"

#include <vector>

namespace scene {

struct MeshPrimitive {
    std::vector<util::Vertex> vertices;
    std::vector<util::Index>  indices;
    Material                  material;
};

} // namespace scene
