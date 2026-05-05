#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace util {

struct Vertex {
    glm::vec3 position{};
    glm::vec2 uv{};
    glm::vec3 normal{};
};

using Index = std::uint32_t;

} // namespace util
