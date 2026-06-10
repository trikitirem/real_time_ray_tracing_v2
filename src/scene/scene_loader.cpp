#include "scene/scene_loader.hpp"

#include "engine/benchmark_constants.hpp"
#include "scene/gltf_loader.hpp"
#include "scene/primitives/cube.hpp"
#include "scene/primitives/plane.hpp"
#include "util/asset_root.hpp"

#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

namespace scene {

namespace {

glm::vec3 read_vec3(const nlohmann::json& j)
{
    if (!j.is_array() || j.size() < 3) {
        throw std::runtime_error("Expected vec3 array with 3 elements");
    }
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

SceneKind parse_scene_kind(const std::string& kind)
{
    if (kind == "viewing") {
        return SceneKind::Viewing;
    }
    if (kind == "benchmark") {
        return SceneKind::Benchmark;
    }
    throw std::runtime_error("Unknown scene kind: " + kind);
}

CameraPreset parse_camera_preset(const nlohmann::json& j)
{
    CameraPreset preset{};
    preset.name           = j.at("name").get<std::string>();
    preset.position       = read_vec3(j.at("position"));
    preset.euler_degrees  = read_vec3(j.at("euler_degrees"));
    return preset;
}

PrimitiveConfig parse_primitive(const nlohmann::json& j)
{
    PrimitiveConfig prim{};
    prim.type      = j.at("type").get<std::string>();
    prim.position      = j.contains("position") ? read_vec3(j.at("position")) : glm::vec3(0.0f);
    prim.euler_degrees = j.contains("euler_degrees") ? read_vec3(j.at("euler_degrees"))
                                                       : glm::vec3(0.0f);
    prim.scale         = j.contains("scale") ? read_vec3(j.at("scale")) : glm::vec3(1.0f);
    prim.color     = j.contains("color") ? read_vec3(j.at("color")) : glm::vec3(1.0f);
    prim.metalness = j.value("metalness", 0.0f);
    prim.roughness = j.value("roughness", 0.5f);
    if (j.contains("texture")) {
        prim.texture = j.at("texture").get<std::string>();
    }
    return prim;
}

ModelConfig parse_model(const nlohmann::json& j)
{
    ModelConfig model{};
    model.gltf_path = j.at("gltf_path").get<std::string>();
    model.position      = j.contains("position") ? read_vec3(j.at("position")) : glm::vec3(0.0f);
    model.euler_degrees = j.contains("euler_degrees") ? read_vec3(j.at("euler_degrees"))
                                                        : glm::vec3(0.0f);
    model.scale         = j.contains("scale") ? read_vec3(j.at("scale")) : glm::vec3(1.0f);
    return model;
}

Model build_primitive(const PrimitiveConfig& prim, const std::filesystem::path& /*asset_root*/)
{
    Material mat{};
    mat.base_color = prim.color;
    mat.metalness  = prim.metalness;
    mat.roughness  = prim.roughness;
    if (!prim.texture.empty()) {
        const auto p = util::resolve_asset(prim.texture);
        mat.texture_location
            = util::AssetLocation{ p.parent_path(), p.filename().string() };
    }

    Transform xf{};
    xf.translate(prim.position.x, prim.position.y, prim.position.z);
    xf.apply_euler_degrees(prim.euler_degrees);
    xf.scale(prim.scale.x, prim.scale.y, prim.scale.z);

    if (prim.type == "cube") {
        return primitives::make_cube(1.0f, 1.0f, 1.0f, mat, xf);
    }
    if (prim.type == "plane") {
        return primitives::make_plane(1.0f, 1.0f, mat, xf);
    }
    throw std::runtime_error("Unknown primitive type: " + prim.type);
}

void validate_config(const SceneConfig& cfg)
{
    if (cfg.kind == SceneKind::Benchmark && !cfg.benchmark_path.has_value()) {
        std::cerr << "[Scene] Warning: benchmark scene '" << cfg.name
                  << "' has no benchmark_path — camera will stay fixed during benchmark.\n";
    }

    for (const auto& prim : cfg.primitives) {
        if (prim.type != "cube" && prim.type != "plane") {
            throw std::runtime_error("Scene '" + cfg.name + "': unknown primitive type '"
                                     + prim.type + "'");
        }
    }

    if (cfg.stress.enabled) {
        if (cfg.stress.initial_count < 0) {
            throw std::runtime_error("Scene '" + cfg.name
                                     + "': stress.initial_count must be >= 0");
        }
        if (cfg.stress.step <= 0) {
            throw std::runtime_error("Scene '" + cfg.name + "': stress.step must be > 0");
        }
        if (cfg.stress.metalness < 0.0f || cfg.stress.metalness > 1.0f) {
            throw std::runtime_error("Scene '" + cfg.name
                                     + "': stress.metalness must be in [0, 1]");
        }
        if (cfg.stress.roughness < 0.0f || cfg.stress.roughness > 1.0f) {
            throw std::runtime_error("Scene '" + cfg.name
                                     + "': stress.roughness must be in [0, 1]");
        }
    }
}

} // namespace

SceneConfig load_scene_config(const std::filesystem::path& json_path)
{
    std::ifstream file(json_path);
    if (!file) {
        throw std::runtime_error("Failed to open scene config: " + json_path.string());
    }

    const nlohmann::json j = nlohmann::json::parse(file);

    SceneConfig cfg{};
    cfg.name = j.at("name").get<std::string>();
    cfg.kind = parse_scene_kind(j.at("kind").get<std::string>());

    if (j.contains("benchmark")) {
        cfg.benchmark.duration_seconds
            = j.at("benchmark").value("duration_seconds", engine::kDefaultBenchmarkDurationS);
    }

    if (j.contains("camera_presets")) {
        for (const auto& preset_j : j.at("camera_presets")) {
            cfg.camera_presets.push_back(parse_camera_preset(preset_j));
        }
    }

    if (j.contains("initial_camera")) {
        const auto& ic = j.at("initial_camera");
        InitialCamera cam{};
        cam.position      = read_vec3(ic.at("position"));
        cam.euler_degrees = ic.contains("euler_degrees") ? read_vec3(ic.at("euler_degrees"))
                                                         : glm::vec3(0.0f);
        cfg.initial_camera = cam;
    }

    if (j.contains("benchmark_path")) {
        const auto& bp = j.at("benchmark_path");
        BenchmarkPath path{};
        path.from              = read_vec3(bp.at("from"));
        path.to                = read_vec3(bp.at("to"));
        path.look_at           = read_vec3(bp.at("look_at"));
        path.duration_seconds  = bp.value("duration_seconds", 30.0f);
        cfg.benchmark_path     = path;
    }

    if (j.contains("primitives")) {
        for (const auto& prim_j : j.at("primitives")) {
            cfg.primitives.push_back(parse_primitive(prim_j));
        }
    }

    if (j.contains("models")) {
        for (const auto& model_j : j.at("models")) {
            cfg.models.push_back(parse_model(model_j));
        }
    }

    if (j.contains("stress")) {
        const auto& s       = j.at("stress");
        cfg.stress.enabled  = s.value("enabled", false);
        cfg.stress.initial_count = s.value("initial_count", 100);
        cfg.stress.step          = s.value("step", 100);
        cfg.stress.min_count     = s.value("min_count", 0);
        cfg.stress.max_count     = s.value("max_count", 10000);
        cfg.stress.spread        = s.value("spread", 10.0f);
        cfg.stress.rng_seed      = s.value("rng_seed", 42);
        if (s.contains("color")) {
            cfg.stress.color = read_vec3(s.at("color"));
        }
        cfg.stress.metalness = s.value("metalness", 0.0f);
        cfg.stress.roughness = s.value("roughness", 0.5f);
    }

    validate_config(cfg);
    return cfg;
}

SceneStats count_scene_stats(const Scene& scene, const int stress_count)
{
    SceneStats stats{};
    stats.object_count = static_cast<int>(scene.model_count());
    stats.stress_count = stress_count;

    for (const auto& model : scene.models()) {
        for (const auto& prim : model.mesh.primitives) {
            stats.vertex_count += static_cast<int>(prim.vertices.size());
            stats.triangle_count += static_cast<int>(prim.indices.size() / 3);
        }
    }

    return stats;
}

std::pair<Scene, SceneStats> build_scene(const SceneConfig&           cfg,
                                           const std::filesystem::path& /*asset_root*/,
                                           const int                    stress_count_override)
{
    Scene scene{};

    for (const auto& prim : cfg.primitives) {
        scene.add_model(build_primitive(prim, util::asset_root()));
    }

    for (const auto& model_cfg : cfg.models) {
        const auto gltf_path = util::resolve_asset(model_cfg.gltf_path);
        for (auto& model : load_gltf_models(gltf_path, model_cfg)) {
            scene.add_model(std::move(model));
        }
    }

    int stress_count = 0;
    if (cfg.stress.enabled) {
        stress_count = stress_count_override >= 0 ? stress_count_override : cfg.stress.initial_count;

        Material stress_mat{};
        stress_mat.base_color = cfg.stress.color;
        stress_mat.metalness  = cfg.stress.metalness;
        stress_mat.roughness  = cfg.stress.roughness;

        std::mt19937                          rng(static_cast<std::uint32_t>(cfg.stress.rng_seed));
        std::uniform_real_distribution<float> dist(-cfg.stress.spread, cfg.stress.spread);

        for (int i = 0; i < stress_count; ++i) {
            Transform xf{};
            xf.translate(dist(rng), dist(rng), dist(rng));
            scene.add_model(primitives::make_cube(1.0f, 1.0f, 1.0f, stress_mat, xf));
        }
    }

    return { std::move(scene), count_scene_stats(scene, stress_count) };
}

} // namespace scene
