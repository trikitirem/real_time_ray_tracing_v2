#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.hpp>

#include "renderer/shared/buffers/gpu_buffer.hpp"
#include "renderer/shared/textures/texture_resource.hpp"

namespace renderer::ray_tracing {

inline constexpr std::uint32_t kNoTexture = std::numeric_limits<std::uint32_t>::max();

struct RtDrawItem {
    vk::Buffer vertex_buffer{};
    vk::Buffer index_buffer{};
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    std::uint32_t first_index = 0;
    std::int32_t vertex_offset = 0;
    std::uint32_t texture_index = kNoTexture;
    glm::mat4 model_matrix = glm::mat4(1.0f);
    std::uint32_t material_index = 0;
};

struct ReflectionInstanceLutEntry {
    std::uint32_t material_index = 0;
    std::uint32_t index_offset = 0;
};

struct RtSceneGpuData {
    std::vector<buffers::GpuBuffer> vertex_buffers{};
    std::vector<buffers::GpuBuffer> index_buffers{};
    std::optional<buffers::GpuBuffer> material_buffer{};
    std::optional<buffers::GpuBuffer> reflection_index_buffer{};
    std::optional<buffers::GpuBuffer> reflection_uv_buffer{};
    std::vector<textures::TextureResource> textures{};

    std::vector<RtDrawItem> draw_items{};
    std::vector<ReflectionInstanceLutEntry> reflection_instance_lut{};
    std::vector<glm::vec4> material_albedos{};
    std::vector<vk::ImageView> texture_views{};
    vk::Sampler texture_sampler{};
    std::uint32_t material_count = 0;
    bool valid = false;
};

} // namespace renderer::ray_tracing
