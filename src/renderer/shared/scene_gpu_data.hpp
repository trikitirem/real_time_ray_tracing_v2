#pragma once

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.hpp>

#include "renderer/shared/buffers/gpu_buffer.hpp"
#include "renderer/shared/textures/texture_resource.hpp"

namespace renderer {

struct DrawItem {
    vk::Buffer vertex_buffer{};
    vk::Buffer index_buffer{};
    std::uint32_t index_count = 0;
    std::uint32_t first_index = 0;
    std::int32_t vertex_offset = 0;
    glm::mat4 model_matrix = glm::mat4(1.0f);
    std::uint32_t material_index = 0;
};

struct SceneGpuData {
    std::vector<buffers::GpuBuffer> vertex_buffers{};
    std::vector<buffers::GpuBuffer> index_buffers{};
    std::vector<textures::TextureResource> textures{};

    std::vector<DrawItem> draw_items{};
    vk::Buffer material_buffer{};
    std::vector<vk::ImageView> texture_views{};
    vk::Sampler texture_sampler{};
    std::uint32_t material_count = 0;
    bool valid = false;
};

} // namespace renderer
