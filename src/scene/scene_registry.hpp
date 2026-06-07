#pragma once

#include <string_view>

namespace scene {

enum class SceneName {
    Test,
    GraphicsTest,
    StressTest,
};

[[nodiscard]] std::string_view scene_json_path(SceneName name);
[[nodiscard]] std::string_view scene_display_name(SceneName name);

} // namespace scene
