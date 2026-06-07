#include "scene/gltf_loader.hpp"

#include "util/asset_location.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <stb_image.h>

#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

namespace scene {

namespace {

Transform make_instance_transform(const ModelConfig& config)
{
    Transform xf{};
    xf.translate(config.position.x, config.position.y, config.position.z);
    xf.scale(config.scale.x, config.scale.y, config.scale.z);
    return xf;
}

Material make_material(const tinygltf::Model&      gltf,
                       const std::filesystem::path& gltf_dir,
                       const int                    material_index)
{
    Material mat{};

    if (material_index < 0
        || static_cast<std::size_t>(material_index) >= gltf.materials.size()) {
        return mat;
    }

    const tinygltf::Material& gm = gltf.materials[static_cast<std::size_t>(material_index)];
    const auto&               pbr = gm.pbrMetallicRoughness;

    mat.base_color = glm::vec3(static_cast<float>(pbr.baseColorFactor[0]),
                               static_cast<float>(pbr.baseColorFactor[1]),
                               static_cast<float>(pbr.baseColorFactor[2]));
    mat.metalness  = static_cast<float>(pbr.metallicFactor);
    mat.roughness  = static_cast<float>(pbr.roughnessFactor);

    if (pbr.baseColorTexture.index >= 0
        && static_cast<std::size_t>(pbr.baseColorTexture.index) < gltf.textures.size()) {
        const tinygltf::Texture& tex
            = gltf.textures[static_cast<std::size_t>(pbr.baseColorTexture.index)];
        if (tex.source >= 0
            && static_cast<std::size_t>(tex.source) < gltf.images.size()) {
            const tinygltf::Image& img = gltf.images[static_cast<std::size_t>(tex.source)];
            if (!img.uri.empty()) {
                const auto tex_path = (gltf_dir / img.uri).lexically_normal();
                mat.texture_location = util::AssetLocation{ tex_path.parent_path(),
                                                              tex_path.filename().string() };
            }
        }
    }

    return mat;
}

MeshPrimitive build_primitive(const tinygltf::Model&       gltf,
                              const tinygltf::Primitive&   primitive,
                              const std::filesystem::path& gltf_dir)
{
    const auto pos_it = primitive.attributes.find("POSITION");
    if (pos_it == primitive.attributes.end()) {
        throw std::runtime_error("GLTF primitive missing POSITION attribute");
    }

    const tinygltf::Accessor& pos_accessor
        = gltf.accessors[static_cast<std::size_t>(pos_it->second)];
    const std::size_t vertex_count = static_cast<std::size_t>(pos_accessor.count);

    const int normal_accessor = primitive.attributes.count("NORMAL") > 0
                                    ? primitive.attributes.at("NORMAL")
                                    : -1;
    const int uv_accessor = primitive.attributes.count("TEXCOORD_0") > 0
                                ? primitive.attributes.at("TEXCOORD_0")
                                : -1;

    MeshPrimitive mesh_prim{};
    mesh_prim.material = make_material(gltf, gltf_dir, primitive.material);
    mesh_prim.vertices.reserve(vertex_count);

    for (std::size_t i = 0; i < vertex_count; ++i) {
        util::Vertex vertex{};

        const tinygltf::Accessor& pos_acc
            = gltf.accessors[static_cast<std::size_t>(pos_it->second)];
        const tinygltf::BufferView& pos_view
            = gltf.bufferViews[static_cast<std::size_t>(pos_acc.bufferView)];
        const tinygltf::Buffer& pos_buffer
            = gltf.buffers[static_cast<std::size_t>(pos_view.buffer)];
        const std::size_t pos_offset = static_cast<std::size_t>(
            pos_view.byteOffset + pos_acc.byteOffset + i * 3 * sizeof(float));
        const auto* pos_data
            = reinterpret_cast<const float*>(pos_buffer.data.data() + pos_offset);
        vertex.position = glm::vec3(pos_data[0], pos_data[1], pos_data[2]);

        if (normal_accessor >= 0) {
            const tinygltf::Accessor& n_acc
                = gltf.accessors[static_cast<std::size_t>(normal_accessor)];
            const tinygltf::BufferView& n_view
                = gltf.bufferViews[static_cast<std::size_t>(n_acc.bufferView)];
            const tinygltf::Buffer& n_buffer
                = gltf.buffers[static_cast<std::size_t>(n_view.buffer)];
            const std::size_t n_offset = static_cast<std::size_t>(
                n_view.byteOffset + n_acc.byteOffset + i * 3 * sizeof(float));
            const auto* n_data
                = reinterpret_cast<const float*>(n_buffer.data.data() + n_offset);
            vertex.normal = glm::vec3(n_data[0], n_data[1], n_data[2]);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (uv_accessor >= 0) {
            const tinygltf::Accessor& uv_acc
                = gltf.accessors[static_cast<std::size_t>(uv_accessor)];
            const tinygltf::BufferView& uv_view
                = gltf.bufferViews[static_cast<std::size_t>(uv_acc.bufferView)];
            const tinygltf::Buffer& uv_buffer
                = gltf.buffers[static_cast<std::size_t>(uv_view.buffer)];
            const std::size_t uv_offset = static_cast<std::size_t>(
                uv_view.byteOffset + uv_acc.byteOffset + i * 2 * sizeof(float));
            const auto* uv_data
                = reinterpret_cast<const float*>(uv_buffer.data.data() + uv_offset);
            vertex.uv = glm::vec2(uv_data[0], uv_data[1]);
        }

        mesh_prim.vertices.push_back(vertex);
    }

    if (primitive.indices < 0) {
        throw std::runtime_error("GLTF primitive missing indices");
    }

    const tinygltf::Accessor& index_accessor
        = gltf.accessors[static_cast<std::size_t>(primitive.indices)];
    const tinygltf::BufferView& index_view
        = gltf.bufferViews[static_cast<std::size_t>(index_accessor.bufferView)];
    const tinygltf::Buffer& index_buffer
        = gltf.buffers[static_cast<std::size_t>(index_view.buffer)];

    const std::size_t index_byte_offset
        = static_cast<std::size_t>(index_view.byteOffset + index_accessor.byteOffset);
    const std::size_t index_count = static_cast<std::size_t>(index_accessor.count);

    mesh_prim.indices.reserve(index_count);
    for (std::size_t i = 0; i < index_count; ++i) {
        util::Index index = 0;
        switch (index_accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const auto* data = reinterpret_cast<const std::uint8_t*>(
                index_buffer.data.data() + index_byte_offset);
            index = static_cast<util::Index>(data[i]);
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const auto* data = reinterpret_cast<const std::uint16_t*>(
                index_buffer.data.data() + index_byte_offset);
            index = static_cast<util::Index>(data[i]);
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const auto* data = reinterpret_cast<const std::uint32_t*>(
                index_buffer.data.data() + index_byte_offset);
            index = static_cast<util::Index>(data[i]);
            break;
        }
        default:
            throw std::runtime_error("Unsupported GLTF index component type");
        }
        mesh_prim.indices.push_back(index);
    }

    return mesh_prim;
}

} // namespace

std::vector<Model> load_gltf_models(const std::filesystem::path& gltf_path,
                                    const ModelConfig&           config)
{
    tinygltf::TinyGLTF loader{};
    tinygltf::Model    gltf{};
    std::string        warn;
    std::string        err;

    const std::string path_str = gltf_path.string();
    const bool loaded          = gltf_path.extension() == ".glb"
                                     ? loader.LoadBinaryFromFile(&gltf, &err, &warn, path_str)
                                     : loader.LoadASCIIFromFile(&gltf, &err, &warn, path_str);
    if (!warn.empty()) {
        std::cerr << "[Scene] GLTF warning (" << path_str << "): " << warn << '\n';
    }
    if (!loaded) {
        throw std::runtime_error("Failed to load GLTF: " + path_str + " — " + err);
    }

    const std::filesystem::path gltf_dir = gltf_path.parent_path();
    const Transform             instance_xf = make_instance_transform(config);

    std::vector<Model> models{};
    models.reserve(gltf.meshes.size());

    for (const tinygltf::Mesh& mesh : gltf.meshes) {
        Model model{};
        model.transform = instance_xf;
        model.mesh.primitives.reserve(mesh.primitives.size());

        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            model.mesh.primitives.push_back(build_primitive(gltf, primitive, gltf_dir));
        }

        if (!model.mesh.primitives.empty()) {
            models.push_back(std::move(model));
        }
    }

    return models;
}

} // namespace scene
