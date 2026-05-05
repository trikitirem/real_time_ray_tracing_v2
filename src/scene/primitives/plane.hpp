#pragma once

#include "scene/material.hpp"
#include "scene/mesh.hpp"
#include "scene/mesh_primitive.hpp"
#include "scene/model.hpp"
#include "scene/transform.hpp"
#include "util/vertex.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace scene::primitives {

/// Axis-aligned plane in XZ, normal +Y (matches Zig `Plane.initModel`).
[[nodiscard]] inline Model make_plane(float width,
                                    float height,
                                    const Material&           material,
                                    const Transform& transform = {})
{
    const float               hx  = width * 0.5f;
    const float               hz  = height * 0.5f;
    const glm::vec3           n{ 0.0f, 1.0f, 0.0f };
    const glm::vec3           positions[4]{
        { -hx, 0.0f, -hz },
        { hx,  0.0f, -hz },
        { hx,  0.0f, hz },
        { -hx, 0.0f, hz },
    };
    const glm::vec2 uvs[4]{
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f },
    };

    MeshPrimitive prim{};
    prim.material = material;
    prim.vertices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        prim.vertices.push_back(util::Vertex{ positions[i], uvs[i], n });
    }
    prim.indices = { 0, 1, 2, 0, 2, 3 };

    Mesh mesh{};
    mesh.primitives.push_back(std::move(prim));
    return Model{ std::move(mesh), transform };
}

} // namespace scene::primitives
