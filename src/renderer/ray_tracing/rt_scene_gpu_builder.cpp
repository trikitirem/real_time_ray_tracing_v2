#include "renderer/ray_tracing/rt_scene_gpu_builder.hpp"

#include "renderer/ray_tracing/rt_gpu_types.hpp"
#include "renderer/shared/buffers/buffer_kind.hpp"
#include "renderer/shared/device_context.hpp"
#include "scene/mesh_primitive.hpp"
#include "scene/model.hpp"
#include "scene/scene.hpp"
#include "util/vertex.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace renderer::ray_tracing {

namespace {

vk::raii::CommandPool make_command_pool(const DeviceContext& ctx, const vk::raii::Device& device)
{
    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = ctx.graphicsQueueFamily();
    return vk::raii::CommandPool(device, pool_info);
}

std::uint32_t load_texture(DeviceContext& ctx,
                           const vk::raii::Device& device,
                           const vk::raii::CommandPool& command_pool,
                           RtSceneGpuData& out,
                           const util::AssetLocation& location)
{
    out.textures.push_back(textures::TextureResource::load_from_asset_location(
        ctx.physicalDevice(),
        device,
        command_pool,
        ctx.graphicsQueue(),
        location));
    out.texture_views.push_back(*out.textures.back().view());
    if (out.texture_sampler == vk::Sampler{}) {
        out.texture_sampler = *out.textures.back().sampler();
    }
    return static_cast<std::uint32_t>(out.textures.size() - 1);
}

void append_primitive(DeviceContext& ctx,
                      const vk::raii::Device& device,
                      const vk::raii::CommandPool& command_pool,
                      const scene::Model& model,
                      const scene::MeshPrimitive& primitive,
                      RtSceneGpuData& out,
                      std::vector<std::uint32_t>& reflection_indices,
                      std::vector<glm::vec2>& reflection_uvs,
                      std::vector<glm::vec4>& reflection_normals)
{
    if (primitive.vertices.empty() || primitive.indices.empty()) {
        return;
    }

    out.vertex_buffers.push_back(buffers::GpuBuffer::from_span(
        ctx.physicalDevice(),
        device,
        command_pool,
        ctx.graphicsQueue(),
        buffers::BufferKind::rt_vertex_as_input,
        std::span<const util::Vertex>(primitive.vertices.data(), primitive.vertices.size())));
    out.index_buffers.push_back(buffers::GpuBuffer::from_span(
        ctx.physicalDevice(),
        device,
        command_pool,
        ctx.graphicsQueue(),
        buffers::BufferKind::rt_index_as_input,
        std::span<const util::Index>(primitive.indices.data(), primitive.indices.size())));

    const auto vb_idx = out.vertex_buffers.size() - 1;
    const auto ib_idx = out.index_buffers.size() - 1;
    const std::uint32_t material_index
        = static_cast<std::uint32_t>(out.material_albedos.size());
    out.material_albedos.push_back(glm::vec4(primitive.material.base_color, 1.0f));
    out.material_metalness.push_back(primitive.material.metalness);
    out.material_roughness.push_back(primitive.material.roughness);

    const std::uint32_t reflection_index_offset = static_cast<std::uint32_t>(reflection_indices.size());
    const std::uint32_t reflection_uv_base = static_cast<std::uint32_t>(reflection_uvs.size());
    reflection_indices.reserve(reflection_indices.size() + primitive.indices.size());
    for (const util::Index local_index : primitive.indices) {
        reflection_indices.push_back(reflection_uv_base + local_index);
    }
    reflection_uvs.reserve(reflection_uvs.size() + primitive.vertices.size());
    reflection_normals.reserve(reflection_normals.size() + primitive.vertices.size());
    const glm::mat3 normal_matrix
        = glm::transpose(glm::inverse(glm::mat3(model.transform.matrix)));
    for (const util::Vertex& vertex : primitive.vertices) {
        reflection_uvs.push_back(vertex.uv);
        const glm::vec3 world_n = glm::normalize(normal_matrix * vertex.normal);
        reflection_normals.push_back(glm::vec4(world_n, 0.0f));
    }

    std::uint32_t texture_index = kNoTexture;
    if (!primitive.material.texture_location.file_name.empty()) {
        texture_index = load_texture(ctx, device, command_pool, out, primitive.material.texture_location);
    }

    out.draw_items.push_back(RtDrawItem{
        .vertex_buffer = *out.vertex_buffers[vb_idx].buffer(),
        .index_buffer = *out.index_buffers[ib_idx].buffer(),
        .vertex_count = static_cast<std::uint32_t>(primitive.vertices.size()),
        .index_count = static_cast<std::uint32_t>(primitive.indices.size()),
        .first_index = 0,
        .vertex_offset = 0,
        .texture_index = texture_index,
        .model_matrix = model.transform.matrix,
        .material_index = material_index,
    });
    out.reflection_instance_lut.push_back(ReflectionInstanceLutEntry{
        .material_index = material_index,
        .index_offset = reflection_index_offset,
    });
}

void finalize_material_buffer(DeviceContext& ctx,
                              const vk::raii::Device& device,
                              const vk::raii::CommandPool& command_pool,
                              RtSceneGpuData& out)
{
    if (out.material_albedos.empty()) {
        return;
    }

    std::vector<MaterialGpu> gpu_materials{};
    gpu_materials.reserve(out.material_albedos.size());
    for (const RtDrawItem& item : out.draw_items) {
        const glm::vec4 albedo = item.material_index < out.material_albedos.size()
                                     ? out.material_albedos[item.material_index]
                                     : glm::vec4(1.0f);
        const float metalness = item.material_index < out.material_metalness.size()
                                    ? out.material_metalness[item.material_index]
                                    : 0.0f;
        const float roughness = item.material_index < out.material_roughness.size()
                                    ? out.material_roughness[item.material_index]
                                    : 0.5f;
        gpu_materials.push_back(MaterialGpu{
            .albedo        = albedo,
            .texture_index = item.texture_index,
            .has_texture   = item.texture_index != kNoTexture ? 1u : 0u,
            .metalness     = metalness,
            .roughness     = roughness,
        });
    }
    out.material_buffer.emplace(buffers::GpuBuffer::from_span(
        ctx.physicalDevice(),
        device,
        command_pool,
        ctx.graphicsQueue(),
        buffers::BufferKind::storage,
        std::span<const MaterialGpu>(gpu_materials.data(), gpu_materials.size())));
}

void finalize_reflection_buffers(DeviceContext& ctx,
                                 const vk::raii::Device& device,
                                 const vk::raii::CommandPool& command_pool,
                                 RtSceneGpuData& out,
                                 const std::vector<std::uint32_t>& reflection_indices,
                                 const std::vector<glm::vec2>& reflection_uvs,
                                 const std::vector<glm::vec4>& reflection_normals)
{
    if (!reflection_indices.empty()) {
        out.reflection_index_buffer.emplace(buffers::GpuBuffer::from_span(
            ctx.physicalDevice(),
            device,
            command_pool,
            ctx.graphicsQueue(),
            buffers::BufferKind::storage,
            std::span<const std::uint32_t>(reflection_indices.data(), reflection_indices.size())));
    }
    if (!reflection_uvs.empty()) {
        out.reflection_uv_buffer.emplace(buffers::GpuBuffer::from_span(
            ctx.physicalDevice(),
            device,
            command_pool,
            ctx.graphicsQueue(),
            buffers::BufferKind::storage,
            std::span<const glm::vec2>(reflection_uvs.data(), reflection_uvs.size())));
    }
    if (!reflection_normals.empty()) {
        out.reflection_normal_buffer.emplace(buffers::GpuBuffer::from_span(
            ctx.physicalDevice(),
            device,
            command_pool,
            ctx.graphicsQueue(),
            buffers::BufferKind::storage,
            std::span<const glm::vec4>(reflection_normals.data(), reflection_normals.size())));
    }
}

} // namespace

ScenePayload RtSceneGpuBuilder::build(DeviceContext& ctx, const scene::Scene& scene)
{
    RtSceneGpuData out{};
    std::vector<std::uint32_t> reflection_indices{};
    std::vector<glm::vec2>     reflection_uvs{};
    std::vector<glm::vec4>     reflection_normals{};
    const vk::raii::Device& device = ctx.device();
    const vk::raii::CommandPool command_pool = make_command_pool(ctx, device);

    for (const scene::Model& model : scene.models()) {
        for (const scene::MeshPrimitive& primitive : model.mesh.primitives) {
            append_primitive(
                ctx,
                device,
                command_pool,
                model,
                primitive,
                out,
                reflection_indices,
                reflection_uvs,
                reflection_normals);
        }
    }

    out.material_count = static_cast<std::uint32_t>(out.material_albedos.size());
    finalize_material_buffer(ctx, device, command_pool, out);
    finalize_reflection_buffers(
        ctx, device, command_pool, out, reflection_indices, reflection_uvs, reflection_normals);
    out.valid = !out.draw_items.empty();
    ScenePayload payload{};
    payload.backend = BackendKind::ray_tracing;
    payload.set(std::move(out));
    return payload;
}

} // namespace renderer::ray_tracing
