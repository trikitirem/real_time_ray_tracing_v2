#pragma once

#include "engine/benchmark_constants.hpp"

#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <vector>

namespace scene {

struct CameraPreset {
    std::string name;
    glm::vec3   position{ 0.0f };
    glm::vec3   euler_degrees{ 0.0f };
};

struct InitialCamera {
    glm::vec3 position{ 0.0f };
    glm::vec3 euler_degrees{ 0.0f };
};

struct BenchmarkSettings {
    float duration_seconds = engine::kDefaultBenchmarkDurationS;
};

struct BenchmarkPath {
    glm::vec3 from{ 0.0f };
    glm::vec3 to{ 0.0f, 0.0f, -5.0f };
    glm::vec3 look_at{ 0.0f };
    float     duration_seconds = 30.0f;
};

enum class SceneKind {
    Viewing,
    Benchmark,
};

struct PrimitiveConfig {
    std::string type;
    glm::vec3   position{ 0.0f };
    glm::vec3   scale{ 1.0f };
    glm::vec3   color{ 1.0f };
    float       metalness  = 0.0f;
    float       roughness  = 0.5f;
    std::string texture;
};

struct ModelConfig {
    std::string gltf_path;
    glm::vec3   position{ 0.0f };
    glm::vec3   scale{ 1.0f };
};

struct StressConfig {
    bool      enabled       = false;
    int       initial_count = 100;
    int       step          = 100;
    int       min_count     = 0;
    int       max_count     = 10000;
    float     spread        = 10.0f;
    glm::vec3 color{ 0.5f };
    int       rng_seed      = 42;
};

struct SceneConfig {
    std::string                  name;
    SceneKind                    kind = SceneKind::Viewing;
    BenchmarkSettings            benchmark{};
    std::vector<CameraPreset>    camera_presets;
    std::optional<InitialCamera> initial_camera;
    std::optional<BenchmarkPath> benchmark_path;
    std::vector<PrimitiveConfig> primitives;
    std::vector<ModelConfig>     models;
    StressConfig                 stress;
};

} // namespace scene
