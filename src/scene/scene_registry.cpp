#include "scene/scene_registry.hpp"

namespace scene {

std::string_view scene_json_path(const SceneName name)
{
    switch (name) {
    case SceneName::Test:
        return "scenes/test_scene.json";
    case SceneName::GraphicsTest:
        return "scenes/graphics_test.json";
    case SceneName::StressTest:
        return "scenes/stress_test.json";
    }
    return "";
}

std::string_view scene_display_name(const SceneName name)
{
    switch (name) {
    case SceneName::Test:
        return "Test";
    case SceneName::GraphicsTest:
        return "GraphicsTest";
    case SceneName::StressTest:
        return "StressTest";
    }
    return "Unknown";
}

} // namespace scene
