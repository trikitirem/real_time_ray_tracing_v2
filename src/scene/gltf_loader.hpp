#pragma once

#include "scene/model.hpp"
#include "scene/scene_config.hpp"

#include <filesystem>
#include <vector>

namespace scene {

[[nodiscard]] std::vector<Model> load_gltf_models(const std::filesystem::path& gltf_path,
                                                  const ModelConfig&           config);

} // namespace scene
