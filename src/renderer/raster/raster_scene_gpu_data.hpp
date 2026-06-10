#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.hpp>

#include "renderer/shared/buffers/gpu_buffer.hpp"
#include "renderer/shared/textures/texture_resource.hpp"

namespace renderer::raster {

inline constexpr std::uint32_t kNoTexture = std::numeric_limits<std::uint32_t>::max();

struct InstancedDrawItem {
    vk::Buffer vertex_buffer{};
    vk::Buffer index_buffer{};
    vk::Buffer instance_buffer{};
    std::uint32_t index_count    = 0;
    std::uint32_t instance_count = 0;
    std::uint32_t material_index = 0;
    std::uint32_t texture_index  = kNoTexture;
};

struct RasterDrawItem {
    vk::Buffer vertex_buffer{};
    vk::Buffer index_buffer{};
    std::uint32_t index_count = 0;
    std::uint32_t first_index = 0;
    std::int32_t vertex_offset = 0;
    std::uint32_t texture_index = kNoTexture;
    glm::mat4 model_matrix = glm::mat4(1.0f);
    std::uint32_t material_index = 0;
};

struct RasterSceneGpuData {
    std::vector<buffers::GpuBuffer> vertex_buffers{};
    std::vector<buffers::GpuBuffer> index_buffers{};
    std::vector<buffers::GpuBuffer> instance_buffers{};
    std::vector<textures::TextureResource> textures{};

    std::vector<RasterDrawItem>    draw_items{};
    std::vector<InstancedDrawItem> instanced_items{};
    std::vector<glm::vec4> material_albedos{};
    std::vector<float>     material_roughness{};
    vk::Buffer material_buffer{};
    std::vector<vk::ImageView> texture_views{};
    vk::Sampler texture_sampler{};
    std::uint32_t material_count = 0;
    bool valid = false;
};

inline bool scene_has_draw_work(const RasterSceneGpuData& data)
{
    return !data.draw_items.empty() || !data.instanced_items.empty();
}

} // namespace renderer::raster
