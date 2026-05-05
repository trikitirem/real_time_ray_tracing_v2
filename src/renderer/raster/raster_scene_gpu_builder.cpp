#include "renderer/raster/raster_scene_gpu_builder.hpp"

#include "renderer/shared/buffers/buffer_kind.hpp"
#include "renderer/shared/device_context.hpp"
#include "scene/mesh_primitive.hpp"
#include "scene/model.hpp"
#include "scene/scene.hpp"
#include "util/vertex.hpp"

#include <cstdint>
#include <span>

namespace renderer::raster {

SceneGpuData RasterSceneGpuBuilder::build(DeviceContext& ctx, const scene::Scene& scene)
{
    SceneGpuData out{};

    const vk::raii::Device& device = ctx.device();
    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = ctx.graphicsQueueFamily();
    const vk::raii::CommandPool command_pool(device, pool_info);
    auto load_texture = [&](const util::AssetLocation& location) -> std::uint32_t {
        out.textures.push_back(textures::TextureResource::load_from_asset_location(
            ctx.physicalDevice(),
            device,
            command_pool,
            ctx.graphicsQueue(),
            location));
        out.texture_views.push_back(*out.textures.back().view());
        return static_cast<std::uint32_t>(out.textures.size() - 1);
    };

    for (const scene::Model& model : scene.models()) {
        for (const scene::MeshPrimitive& primitive : model.mesh.primitives) {
            if (primitive.vertices.empty() || primitive.indices.empty()) {
                continue;
            }

            out.vertex_buffers.push_back(buffers::GpuBuffer::from_span(
                ctx.physicalDevice(),
                device,
                command_pool,
                ctx.graphicsQueue(),
                buffers::BufferKind::vertex,
                std::span<const util::Vertex>(primitive.vertices.data(), primitive.vertices.size())));
            out.index_buffers.push_back(buffers::GpuBuffer::from_span(
                ctx.physicalDevice(),
                device,
                command_pool,
                ctx.graphicsQueue(),
                buffers::BufferKind::index,
                std::span<const util::Index>(primitive.indices.data(), primitive.indices.size())));

            const auto vb_idx = out.vertex_buffers.size() - 1;
            const auto ib_idx = out.index_buffers.size() - 1;
            const std::uint32_t material_index
                = static_cast<std::uint32_t>(out.material_albedos.size());
            out.material_albedos.push_back(glm::vec4(primitive.material.base_color, 1.0f));
            std::uint32_t texture_index = kNoTexture;
            if (!primitive.material.texture_location.file_name.empty()) {
                texture_index = load_texture(primitive.material.texture_location);
                if (out.texture_sampler == vk::Sampler{}) {
                    out.texture_sampler = *out.textures.back().sampler();
                }
            }
            out.draw_items.push_back(DrawItem{
                .vertex_buffer = *out.vertex_buffers[vb_idx].buffer(),
                .index_buffer = *out.index_buffers[ib_idx].buffer(),
                .index_count = static_cast<std::uint32_t>(primitive.indices.size()),
                .first_index = 0,
                .vertex_offset = 0,
                .texture_index = texture_index,
                .model_matrix = model.transform.matrix,
                .material_index = material_index,
            });
        }
    }

    out.material_count = static_cast<std::uint32_t>(out.material_albedos.size());
    out.valid = !out.draw_items.empty();
    return out;
}

} // namespace renderer::raster
