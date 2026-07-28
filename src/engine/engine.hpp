#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "engine/benchmark.hpp"
#include "engine/benchmark_session.hpp"
#include "engine/camera.hpp"
#include "engine/camera_animator.hpp"
#include "engine/input_controller.hpp"
#include "engine/window.hpp"
#include "renderer/shared/device_config.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/renderer.hpp"
#include "scene/scene.hpp"
#include "scene/scene_config.hpp"
#include "scene/scene_loader.hpp"
#include "scene/scene_registry.hpp"

namespace engine {

class Engine {
  public:
    explicit Engine(bool useRasterBackend);
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    int run();

  private:
    void initWindow();
    void initVulkan();
    void mainLoop();

    void load_scene(scene::SceneName name);
    void load_scene_content(scene::SceneName name);
    void reload_scene_gpu();
    void adjust_stress_count(int delta);
    void rebuild_stress_scene(int count);
    void toggle_backend();
    void apply_camera_preset(const scene::CameraPreset& preset);
    void log_startup_info() const;
    void handle_scene_switch_input();
    void handle_benchmark_input(float frame_dt);
    void handle_stress_input();
    void handle_backend_input();
    void handle_camera_input(float frame_dt);
    void start_stress_suite();
    void advance_stress_suite();
    void start_diagnostic_suite();
    void advance_diagnostic_suite();
    void begin_diagnostic_config(std::size_t config_index);
    void write_current_frame_trace();
    void apply_backend_key(const std::string& backend_key);
    [[nodiscard]] BenchmarkMeta make_benchmark_meta() const;
    [[nodiscard]] std::string session_backend_key() const;
    [[nodiscard]] float measure_frame_delta();

    const bool useRasterBackendFromMain_;
    renderer::DeviceConfig rasterConfig_;
    renderer::DeviceConfig rayTracingConfig_;

    Window window_;
    renderer::DeviceContext deviceContext_;
    std::unique_ptr<renderer::Renderer> renderer_;
    scene::Scene scene_{};
    Camera camera_{};
    InputController input_{};

    scene::SceneName current_scene_name_{scene::SceneName::Test};
    scene::SceneConfig current_scene_config_{};
    scene::SceneStats scene_stats_{};
    std::vector<scene::CameraPreset> camera_presets_{};
    int current_camera_preset_ = -1;

    bool rt_extensions_supported_ = false;
    bool backend_toggle_enabled_ = false;
    bool active_backend_is_raster_ = true;
    bool is_rendering_paused_ = false;
    bool camera_movement_locked_ = false;

    struct SuiteBackendConfig {
        bool is_raster;
        bool rt_reflections;
    };

    int current_stress_count_ = 0;
    bool stress_suite_active_ = false;
    int suite_run_index_ = 0;
    std::vector<SuiteBackendConfig> pending_suite_backends_;

    // Diagnostic suite (F6): a short, explicit list of backend/object-count pairs that also exports
    // a per-frame CSV trace. Kept separate from the full stress suite so the latter is unaffected.
    bool diagnostic_suite_active_ = false;
    std::size_t diagnostic_config_index_ = 0;
    int diagnostic_run_index_ = 0;

    bool rt_reflections_override_ = true;

    Benchmark benchmark_{};
    std::vector<renderer::GpuTimeSample> gpu_sample_scratch_{};
    CameraAnimator animator_{};
    BenchmarkSession session_{};

    bool framebufferResized_ = false;

    using Clock = std::chrono::steady_clock;
    Clock::time_point last_frame_time_{Clock::now()};
};

} // namespace engine
