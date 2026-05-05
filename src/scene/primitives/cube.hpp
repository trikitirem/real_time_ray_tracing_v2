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

namespace detail {

inline void append_quad(std::vector<util::Vertex>& vertices,
                        std::vector<util::Index>&   indices,
                        glm::vec3                     v0,
                        glm::vec3                     v1,
                        glm::vec3                     v2,
                        glm::vec3                     v3,
                        glm::vec3                     normal)
{
    const util::Index base = static_cast<util::Index>(vertices.size());
    vertices.push_back(util::Vertex{ v0, glm::vec2(0.0f, 0.0f), normal });
    vertices.push_back(util::Vertex{ v1, glm::vec2(1.0f, 0.0f), normal });
    vertices.push_back(util::Vertex{ v2, glm::vec2(1.0f, 1.0f), normal });
    vertices.push_back(util::Vertex{ v3, glm::vec2(0.0f, 1.0f), normal });

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

} // namespace detail

/// Axis-aligned box centered at origin; `width` / `height` / `depth` are full extents (x / y / z).
[[nodiscard]] inline Model make_cube(float width,
                                     float height,
                                     float depth,
                                     const Material&           material,
                                     const Transform& transform = {})
{
    const float hx = width * 0.5f;
    const float hy = height * 0.5f;
    const float hz = depth * 0.5f;

    MeshPrimitive prim{};
    prim.material = material;
    prim.vertices.reserve(24);
    prim.indices.reserve(36);

    // +Z
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(-hx, -hy, hz),
                        glm::vec3(hx, -hy, hz),
                        glm::vec3(hx, hy, hz),
                        glm::vec3(-hx, hy, hz),
                        glm::vec3(0.0f, 0.0f, 1.0f));
    // -Z
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(hx, -hy, -hz),
                        glm::vec3(-hx, -hy, -hz),
                        glm::vec3(-hx, hy, -hz),
                        glm::vec3(hx, hy, -hz),
                        glm::vec3(0.0f, 0.0f, -1.0f));
    // +X
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(hx, hy, hz),
                        glm::vec3(hx, -hy, hz),
                        glm::vec3(hx, -hy, -hz),
                        glm::vec3(hx, hy, -hz),
                        glm::vec3(1.0f, 0.0f, 0.0f));
    // -X
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(-hx, hy, -hz),
                        glm::vec3(-hx, -hy, -hz),
                        glm::vec3(-hx, -hy, hz),
                        glm::vec3(-hx, hy, hz),
                        glm::vec3(-1.0f, 0.0f, 0.0f));
    // +Y
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(-hx, hy, hz),
                        glm::vec3(hx, hy, hz),
                        glm::vec3(hx, hy, -hz),
                        glm::vec3(-hx, hy, -hz),
                        glm::vec3(0.0f, 1.0f, 0.0f));
    // -Y
    detail::append_quad(prim.vertices,
                        prim.indices,
                        glm::vec3(-hx, -hy, hz),
                        glm::vec3(-hx, -hy, -hz),
                        glm::vec3(hx, -hy, -hz),
                        glm::vec3(hx, -hy, hz),
                        glm::vec3(0.0f, -1.0f, 0.0f));

    Mesh mesh{};
    mesh.primitives.push_back(std::move(prim));
    return Model{ std::move(mesh), transform };
}

} // namespace scene::primitives
