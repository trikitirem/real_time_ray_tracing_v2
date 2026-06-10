#pragma once

#include "scene/scene.hpp"
#include "scene/scene_config.hpp"

#include <filesystem>
#include <utility>

namespace scene {

struct SceneStats {
    int object_count   = 0;
    int vertex_count   = 0;
    int triangle_count = 0;
    int stress_count   = 0;
};

[[nodiscard]] SceneConfig load_scene_config(const std::filesystem::path& json_path);

[[nodiscard]] float compute_shadow_half_extent(const SceneConfig& cfg);

[[nodiscard]] std::pair<Scene, SceneStats> build_scene(const SceneConfig&           cfg,
                                                       const std::filesystem::path& asset_root,
                                                       int stress_count_override = -1);

[[nodiscard]] SceneStats count_scene_stats(const Scene& scene, int stress_count = 0);

} // namespace scene
