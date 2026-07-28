#include "engine/engine.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "engine/delta_time.hpp"
#include "engine/frame_trace_writer.hpp"
#include "renderer/raster/device_config.hpp"
#include "renderer/ray_tracing/device_config.hpp"
#include "util/asset_root.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace engine {

namespace {

constexpr std::uint32_t kInitialWidth = 1920;
constexpr std::uint32_t kInitialHeight = 1080;

glm::quat euler_degrees_to_quat(const glm::vec3& euler_deg) {
    const glm::vec3 rad = glm::radians(euler_deg);
    const glm::quat yaw_rot = glm::angleAxis(rad.y, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitch_rot = glm::angleAxis(rad.x, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat roll_rot = glm::angleAxis(rad.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::normalize(yaw_rot * pitch_rot * roll_rot);
}

float elapsed_seconds_since(const std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

const char* backend_name(const bool is_raster) {
    return is_raster ? "raster" : "rt";
}

} // namespace

Engine::Engine(const bool useRasterBackend)
    : useRasterBackendFromMain_(useRasterBackend),
      rasterConfig_(renderer::raster::makeRasterDeviceConfig()),
      rayTracingConfig_(renderer::ray_tracing::makeRayTracingDeviceConfig()) {
    load_scene_content(scene::SceneName::Test);
}

int Engine::run() {
    initWindow();
    initVulkan();
    log_startup_info();
    mainLoop();
    return 0;
}

void Engine::load_scene_content(const scene::SceneName name) {
    current_scene_name_ = name;
    scene_ = scene::Scene{};

    const auto json_path = util::resolve_asset(scene::scene_json_path(name));
    current_scene_config_ = scene::load_scene_config(json_path);
    if (name == scene::SceneName::StressTest && current_scene_config_.stress.enabled) {
        current_stress_count_ = current_scene_config_.stress.initial_count;
    } else {
        current_stress_count_ = 0;
    }

    auto [built_scene, stats] =
        scene::build_scene(current_scene_config_, util::asset_root(),
                           name == scene::SceneName::StressTest ? current_stress_count_ : -1);
    scene_ = std::move(built_scene);
    scene_stats_ = stats;

    camera_presets_ = current_scene_config_.camera_presets;
    current_camera_preset_ = -1;

    if (current_scene_config_.initial_camera.has_value()) {
        const auto& cam = *current_scene_config_.initial_camera;
        camera_.position = cam.position;
        camera_.orientation = euler_degrees_to_quat(cam.euler_degrees);
        camera_.reset_pitch_state();
    }
}

void Engine::initWindow() {
    window_.init(kInitialWidth, kInitialHeight, "real_time_ray_tracing_v2");
    window_.setResizeCallback([this](int /*w*/, int /*h*/) { framebufferResized_ = true; });
    input_.attach(window_.handle());
}

void Engine::initVulkan() {
    rt_extensions_supported_ = renderer::probeRayTracingSupport(window_.handle());
    backend_toggle_enabled_ = rt_extensions_supported_;

    const renderer::DeviceConfig& cfg =
        rt_extensions_supported_ ? rayTracingConfig_ : rasterConfig_;

    if (!rt_extensions_supported_ && !useRasterBackendFromMain_) {
        std::cerr << "[Engine] Ray tracing requested but not available — falling back to "
                     "raster.\n";
        active_backend_is_raster_ = true;
    } else {
        active_backend_is_raster_ = useRasterBackendFromMain_;
    }

    deviceContext_.init(window_.handle(), cfg);

    renderer_ = std::make_unique<renderer::Renderer>(window_.handle(), deviceContext_,
                                                     active_backend_is_raster_);
    reload_scene_gpu();
    renderer_->set_camera(camera_);
}

void Engine::log_startup_info() const {
    const auto& props = deviceContext_.physicalDevice().getProperties();
    std::cout << "[Engine] GPU: " << props.deviceName << '\n';
    std::cout << "[Engine] RT extensions: "
              << (rt_extensions_supported_ ? "supported" : "not supported") << '\n';
    if (backend_toggle_enabled_) {
        std::cout << "[Engine] Backend toggle (F7): enabled\n";
    } else {
        std::cout << "[Engine] Backend toggle (F7): disabled (raster only)\n";
    }
    std::cout << "[Engine] Active backend: " << backend_name(active_backend_is_raster_) << '\n';
    std::cout << "[Engine] Scene: " << scene::scene_display_name(current_scene_name_) << '\n';
    if (deviceContext_.gpuTimingSupported()) {
        std::cout << "[Engine] GPU timing: enabled (timestampPeriod="
                  << deviceContext_.timestampPeriod()
                  << " ns, validBits=" << deviceContext_.timestampValidBits() << ")\n";
    } else {
        std::cout << "[Engine] GPU timing: unavailable (timestampValidBits="
                  << deviceContext_.timestampValidBits() << ")\n";
    }
    std::cout << "[Engine] Present mode: " << renderer_->present_mode_string() << '\n';
}

void Engine::reload_scene_gpu() {
    is_rendering_paused_ = true;
    std::cout << "[Engine] Pausing render, waiting for GPU...\n";
    deviceContext_.device().waitIdle();
    renderer_->set_shadow_half_extent(scene::compute_shadow_half_extent(current_scene_config_));
    renderer_->load_scene(scene_);
    is_rendering_paused_ = false;
}

void Engine::load_scene(const scene::SceneName name) {
    if (benchmark_.is_running()) {
        std::cout << "[Scene] Cannot switch scene — benchmark is running. Press F5 to stop.\n";
        return;
    }
    if (is_rendering_paused_) {
        std::cout << "[Scene] Cannot switch scene — operation in progress.\n";
        return;
    }
    if (name == current_scene_name_) {
        std::cout << "[Scene] Already on: " << scene::scene_display_name(name) << '\n';
        return;
    }

    std::cout << "[Scene] Loading: " << scene::scene_display_name(name) << "...\n";
    const auto t0 = Clock::now();

    load_scene_content(name);

    reload_scene_gpu();

    const float elapsed_s = elapsed_seconds_since(t0);
    std::cout << "[Scene] Loaded: " << scene::scene_display_name(name) << " — "
              << scene_stats_.object_count << " objects, " << scene_stats_.vertex_count
              << " vertices, " << scene_stats_.triangle_count << " triangles (" << elapsed_s
              << "s)\n";
}

void Engine::apply_camera_preset(const scene::CameraPreset& preset) {
    camera_.position = preset.position;
    camera_.orientation = euler_degrees_to_quat(preset.euler_degrees);
    camera_.reset_pitch_state();
}

void Engine::rebuild_stress_scene(const int count) {
    std::cout << "[Stress] Rebuilding scene: " << current_stress_count_ << " -> " << count
              << " objects...\n";
    const auto t0 = Clock::now();

    current_stress_count_ = count;
    scene_ = scene::Scene{};
    auto [built_scene, stats] =
        scene::build_scene(current_scene_config_, util::asset_root(), current_stress_count_);
    scene_ = std::move(built_scene);
    scene_stats_ = stats;
    reload_scene_gpu();

    const float elapsed_s = elapsed_seconds_since(t0);
    std::cout << "[Stress] Done: " << current_stress_count_ << " objects, "
              << scene_stats_.vertex_count << " vertices (" << elapsed_s << "s)\n";
}

void Engine::adjust_stress_count(const int delta) {
    if (current_scene_name_ != scene::SceneName::StressTest) {
        std::cout << "[Stress] Cannot adjust — only available on StressTest scene (press F3).\n";
        return;
    }
    if (benchmark_.is_running()) {
        std::cout << "[Stress] Cannot adjust — benchmark is running.\n";
        return;
    }
    if (is_rendering_paused_) {
        std::cout << "[Stress] Cannot adjust — renderer switch in progress.\n";
        return;
    }

    const int next = current_stress_count_ + delta;
    if (next > current_scene_config_.stress.max_count) {
        std::cout << "[Stress] Cannot increase — already at max ("
                  << current_scene_config_.stress.max_count << ").\n";
        return;
    }
    if (next < current_scene_config_.stress.min_count) {
        std::cout << "[Stress] Cannot decrease — already at min ("
                  << current_scene_config_.stress.min_count << ").\n";
        return;
    }

    rebuild_stress_scene(next);
}

void Engine::start_stress_suite() {
    const auto& stress = current_scene_config_.stress;

    // First F5 press: set up the full backend queue and ensure we start from raster.
    if (!session_.is_initialized()) {
        if (!active_backend_is_raster_) {
            toggle_backend();
        }
        rt_reflections_override_ = true;

        pending_suite_backends_.clear();
        if (rt_extensions_supported_) {
            pending_suite_backends_.push_back({false, true});  // rt_full
            pending_suite_backends_.push_back({false, false}); // rt_shadows
        }

        const BenchmarkMeta tmp = make_benchmark_meta();
        session_.init(tmp.gpu_name, tmp.window_width, tmp.window_height);

        const int backends_total = 1 + static_cast<int>(pending_suite_backends_.size());
        const int total_configs = (stress.max_count - stress.initial_count) / stress.step + 1;
        std::cout << "[Suite] Full session: " << backends_total << " backend(s), " << total_configs
                  << " stress configs each, " << kSuiteRunsPerConfig << " runs/config — "
                  << (backends_total * total_configs * kSuiteRunsPerConfig)
                  << " total benchmarks\n";
    }

    const std::string backend_key = session_backend_key();
    const int total_configs = (stress.max_count - stress.initial_count) / stress.step + 1;
    std::cout << "[Suite] Starting backend: " << backend_key << " ("
              << (total_configs * kSuiteRunsPerConfig) << " benchmarks)\n";

    suite_run_index_ = 0;
    rebuild_stress_scene(stress.initial_count);

    BenchmarkMeta meta = make_benchmark_meta();
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta);

    if (current_scene_config_.benchmark_path.has_value()) {
        animator_.start(*current_scene_config_.benchmark_path);
        camera_.reset_pitch_state();
    }

    stress_suite_active_ = true;
    std::cout << "[Suite] Config 1/" << total_configs << " (" << current_stress_count_
              << " objects), run 1/" << kSuiteRunsPerConfig << '\n';
}

void Engine::advance_stress_suite() {
    if (!benchmark_.consume_finished()) {
        return;
    }

    const auto& stress = current_scene_config_.stress;
    const std::string backend_key = session_backend_key();

    session_.add_run(backend_key, current_stress_count_, benchmark_.stats(), suite_run_index_);
    session_.flush();

    if (suite_run_index_ < kSuiteRunsPerConfig - 1) {
        suite_run_index_++;
        BenchmarkMeta meta = make_benchmark_meta();
        renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
        benchmark_.start(meta);
        std::cout << "[Suite] Config (" << current_stress_count_ << " objects), run "
                  << (suite_run_index_ + 1) << '/' << kSuiteRunsPerConfig << '\n';
        return;
    }

    suite_run_index_ = 0;
    const int next = current_stress_count_ + stress.step;

    if (next > stress.max_count) {
        if (pending_suite_backends_.empty()) {
            stress_suite_active_ = false;
            animator_.stop();
            std::cout << "[Suite] All backends complete. Results -> "
                      << session_.output_path().filename().string() << '\n';
        } else {
            const SuiteBackendConfig cfg = pending_suite_backends_.front();
            pending_suite_backends_.erase(pending_suite_backends_.begin());

            if (cfg.is_raster != active_backend_is_raster_) {
                toggle_backend();
            }
            rt_reflections_override_ = cfg.rt_reflections;

            const std::string next_key = session_backend_key();
            const int total_configs = (stress.max_count - stress.initial_count) / stress.step + 1;
            std::cout << "[Suite] Starting backend: " << next_key << " ("
                      << (total_configs * kSuiteRunsPerConfig) << " benchmarks)\n";

            rebuild_stress_scene(stress.initial_count);

            BenchmarkMeta meta = make_benchmark_meta();
            renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
            benchmark_.start(meta);

            if (current_scene_config_.benchmark_path.has_value()) {
                animator_.start(*current_scene_config_.benchmark_path);
                camera_.reset_pitch_state();
            }

            std::cout << "[Suite] Config 1/" << total_configs << " (" << current_stress_count_
                      << " objects), run 1/" << kSuiteRunsPerConfig << '\n';
        }
        return;
    }

    rebuild_stress_scene(next);

    BenchmarkMeta meta = make_benchmark_meta();
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta);

    const int total_configs = (stress.max_count - stress.initial_count) / stress.step + 1;
    const int config_index = (current_stress_count_ - stress.initial_count) / stress.step + 1;
    std::cout << "[Suite] Config " << config_index << '/' << total_configs << " ("
              << current_stress_count_ << " objects), run 1/" << kSuiteRunsPerConfig << '\n';
}

void Engine::apply_backend_key(const std::string& backend_key) {
    const bool want_raster = backend_key == "raster";
    if (want_raster != active_backend_is_raster_) {
        toggle_backend();
    }
    rt_reflections_override_ = backend_key != "rt_shadows";
}

void Engine::begin_diagnostic_config(const std::size_t config_index) {
    const auto& entry = current_scene_config_.diagnostic.configs[config_index];

    apply_backend_key(entry.backend);
    rebuild_stress_scene(entry.stress_count);

    BenchmarkMeta meta = make_benchmark_meta();
    if (entry.duration_seconds.has_value()) {
        // Slow configurations run longer so their 1% low bucket holds more than a couple of frames.
        meta.configured_duration_s = *entry.duration_seconds;
    }
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta, /*collect_frame_trace=*/true);

    if (current_scene_config_.benchmark_path.has_value()) {
        animator_.start(*current_scene_config_.benchmark_path);
        camera_.reset_pitch_state();
    }

    std::cout << "[Diag] Config " << (config_index + 1) << '/'
              << current_scene_config_.diagnostic.configs.size() << " (" << entry.backend << ", "
              << entry.stress_count << " objects, " << meta.configured_duration_s << "s), run "
              << (diagnostic_run_index_ + 1) << '/'
              << current_scene_config_.diagnostic.runs_per_config << '\n';
}

void Engine::write_current_frame_trace() {
    FrameTraceInfo info{};
    info.backend_key = session_backend_key();
    info.stress_count = current_stress_count_;
    info.run_index = diagnostic_run_index_;
    info.session_timestamp = session_.session_timestamp();
    info.gpu_name = benchmark_.meta().gpu_name;
    info.present_mode = benchmark_.meta().present_mode;
    (void)write_frame_trace(info, benchmark_.frame_samples());
}

void Engine::start_diagnostic_suite() {
    const auto& diag = current_scene_config_.diagnostic;
    if (diag.configs.empty()) {
        std::cout << "[Diag] Scene '" << current_scene_config_.name
                  << "' defines no diagnostic configs.\n";
        return;
    }
    if (!renderer_->gpu_timing_enabled()) {
        std::cout << "[Diag] Warning: GPU timing unavailable on this device — gpu_time_ms will be "
                     "empty.\n";
    }

    if (!session_.is_initialized()) {
        const BenchmarkMeta tmp = make_benchmark_meta();
        session_.init(tmp.gpu_name, tmp.window_width, tmp.window_height);
    }

    diagnostic_config_index_ = 0;
    diagnostic_run_index_ = 0;
    diagnostic_suite_active_ = true;

    std::cout << "[Diag] Starting: " << diag.configs.size() << " config(s) x "
              << diag.runs_per_config
              << " run(s) = " << (static_cast<int>(diag.configs.size()) * diag.runs_per_config)
              << " benchmarks, with per-frame CSV traces\n";

    begin_diagnostic_config(diagnostic_config_index_);
}

void Engine::advance_diagnostic_suite() {
    if (!benchmark_.consume_finished()) {
        return;
    }

    const auto& diag = current_scene_config_.diagnostic;

    session_.add_run(session_backend_key(), current_stress_count_, benchmark_.stats(),
                     diagnostic_run_index_);
    session_.flush();
    // Must happen before the next start(), which clears the sample buffer.
    write_current_frame_trace();

    if (diagnostic_run_index_ < diag.runs_per_config - 1) {
        diagnostic_run_index_++;
        begin_diagnostic_config(diagnostic_config_index_);
        return;
    }

    diagnostic_run_index_ = 0;
    diagnostic_config_index_++;

    if (diagnostic_config_index_ >= diag.configs.size()) {
        diagnostic_suite_active_ = false;
        animator_.stop();
        std::cout << "[Diag] Complete. Results -> " << session_.output_path().filename().string()
                  << " + frames_*.csv\n";
        return;
    }

    begin_diagnostic_config(diagnostic_config_index_);
}

void Engine::toggle_backend() {
    if (!backend_toggle_enabled_) {
        std::cout << "[Renderer] Cannot switch to rt — ray tracing extensions not supported on "
                     "this GPU.\n";
        return;
    }
    if (benchmark_.is_running()) {
        std::cout << "[Renderer] Cannot switch — benchmark is running.\n";
        return;
    }
    if (is_rendering_paused_) {
        std::cout << "[Renderer] Cannot switch — scene reload in progress.\n";
        return;
    }

    const bool to_raster = !active_backend_is_raster_;
    std::cout << "[Renderer] Switching: " << backend_name(active_backend_is_raster_) << " -> "
              << backend_name(to_raster) << "...\n";
    const auto t0 = Clock::now();

    is_rendering_paused_ = true;
    deviceContext_.device().waitIdle();
    active_backend_is_raster_ = to_raster;
    renderer_->switch_backend(to_raster);
    is_rendering_paused_ = false;

    const float elapsed_s = elapsed_seconds_since(t0);
    std::cout << "[Renderer] Switched to " << backend_name(to_raster) << " (" << elapsed_s
              << "s). Reloading scene...\n";

    reload_scene_gpu();
    std::cout << "[Scene] Loaded: " << scene::scene_display_name(current_scene_name_) << " — "
              << scene_stats_.object_count << " objects, " << scene_stats_.vertex_count
              << " vertices, " << scene_stats_.triangle_count << " triangles\n";
}

BenchmarkMeta Engine::make_benchmark_meta() const {
    BenchmarkMeta meta{};
    meta.scene_name = std::string(scene::scene_display_name(current_scene_name_));
    meta.object_count = scene_stats_.object_count;
    meta.vertex_count = scene_stats_.vertex_count;
    meta.triangle_count = scene_stats_.triangle_count;
    meta.stress_count = scene_stats_.stress_count;
    meta.stress_rng_seed = current_scene_config_.stress.rng_seed;
    meta.backend = backend_name(active_backend_is_raster_);
    meta.configured_duration_s = current_scene_config_.benchmark.duration_seconds;
    meta.present_mode = renderer_->present_mode_string();
    meta.gpu_name = std::string(deviceContext_.physicalDevice().getProperties().deviceName.data());
    const auto extent = renderer_->swapchain_extent();
    meta.window_width = extent.width;
    meta.window_height = extent.height;
    meta.rt_reflections_enabled = rt_reflections_override_;
    meta.stress_use_texture = current_scene_config_.stress.use_texture;
    return meta;
}

std::string Engine::session_backend_key() const {
    if (active_backend_is_raster_) {
        return "raster";
    }
    return rt_reflections_override_ ? "rt_full" : "rt_shadows";
}

float Engine::measure_frame_delta() {
    const auto now = Clock::now();
    const float dt = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;
    return dt;
}

void Engine::handle_scene_switch_input() {
    if (benchmark_.is_running()) {
        if (input_.pressed_f1() || input_.pressed_f2() || input_.pressed_f3()) {
            std::cout << "[Scene] Cannot switch scene — benchmark is running. Press F5 to stop.\n";
        }
        return;
    }
    if (is_rendering_paused_) {
        if (input_.pressed_f1() || input_.pressed_f2() || input_.pressed_f3()) {
            std::cout << "[Scene] Cannot switch scene — operation in progress.\n";
        }
        return;
    }

    if (input_.pressed_f1()) {
        load_scene(scene::SceneName::Test);
    } else if (input_.pressed_f2()) {
        load_scene(scene::SceneName::GraphicsTest);
    } else if (input_.pressed_f3()) {
        load_scene(scene::SceneName::StressTest);
    }
}

void Engine::handle_benchmark_input(const float /*frame_dt*/) {
    const bool start_full = input_.pressed_f5();
    const bool start_diagnostic = input_.pressed_f6();
    if (!start_full && !start_diagnostic) {
        return;
    }

    if (benchmark_.is_running() || stress_suite_active_ || diagnostic_suite_active_) {
        std::cout << "[Benchmark] Stopped early (" << benchmark_.elapsed_s() << "s / "
                  << (kWarmupDurationS + benchmark_.meta().configured_duration_s) << "s)\n";
        benchmark_.stop();
        animator_.stop();
        stress_suite_active_ = false;
        diagnostic_suite_active_ = false;
        // Drop the partial run: consume_finished() would otherwise let the suite advance one more
        // step on the next frame.
        (void)benchmark_.consume_finished();
        return;
    }

    if (is_rendering_paused_) {
        std::cout << "[Benchmark] Cannot start — scene reload in progress.\n";
        return;
    }

    if (glfwGetWindowAttrib(window_.handle(), GLFW_FOCUSED) == 0) {
        return;
    }

    if (start_diagnostic) {
        if (current_scene_name_ != scene::SceneName::StressTest) {
            std::cout << "[Diag] Diagnostic suite requires the StressTest scene (F3).\n";
            return;
        }
        start_diagnostic_suite();
        return;
    }

    if (current_scene_name_ == scene::SceneName::StressTest) {
        start_stress_suite();
        return;
    }

    BenchmarkMeta meta = make_benchmark_meta();
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta);
    if (current_scene_config_.benchmark_path.has_value()) {
        animator_.start(*current_scene_config_.benchmark_path);
        camera_.reset_pitch_state();
    }
    std::cout << "[Benchmark] Started: " << meta.scene_name << " (" << meta.backend << "), "
              << meta.configured_duration_s << "s, " << meta.object_count << " objects\n";
}

void Engine::handle_stress_input() {
    if (input_.pressed_stress_increase()) {
        adjust_stress_count(current_scene_config_.stress.step);
    } else if (input_.pressed_stress_decrease()) {
        adjust_stress_count(-current_scene_config_.stress.step);
    }
}

void Engine::handle_backend_input() {
    if (input_.pressed_f7()) {
        toggle_backend();
    }
    if (input_.pressed_f8()) {
        if (active_backend_is_raster_) {
            std::cout << "[Renderer] F8 has no effect in raster mode.\n";
        } else {
            rt_reflections_override_ = !rt_reflections_override_;
            std::cout << "[Renderer] RT reflections: "
                      << (rt_reflections_override_ ? "enabled (rt_full)" : "disabled (rt_shadows)")
                      << '\n';
        }
    }
}

void Engine::handle_camera_input(const float frame_dt) {
    if (input_.pressed_camera_lock_toggle()) {
        camera_movement_locked_ = !camera_movement_locked_;
        std::cout << "[Camera] Movement " << (camera_movement_locked_ ? "locked" : "unlocked")
                  << '\n';
    }

    if (input_.pressed_tab()) {
        if (camera_presets_.empty()) {
            std::cout << "[Camera] No camera presets on this scene.\n";
        } else {
            current_camera_preset_ =
                (current_camera_preset_ + 1) % static_cast<int>(camera_presets_.size());
            apply_camera_preset(camera_presets_[static_cast<std::size_t>(current_camera_preset_)]);
            std::cout << "[Camera] Preset: " << camera_presets_[current_camera_preset_].name << " ("
                      << (current_camera_preset_ + 1) << "/" << camera_presets_.size() << ")\n";
        }
    }

    if (input_.pressed_p()) {
        const glm::vec3 euler = glm::degrees(glm::eulerAngles(camera_.orientation));
        std::cout << "[Camera] position: [" << camera_.position.x << ", " << camera_.position.y
                  << ", " << camera_.position.z << "]  euler_degrees: [" << euler.x << ", "
                  << euler.y << ", " << euler.z
                  << "]  (pitch, yaw, roll — paste into JSON as-is)\n";
    }

    if (animator_.is_running()) {
        animator_.update(camera_, frame_dt);
    } else if (!benchmark_.is_running() && !camera_movement_locked_) {
        camera_.update_from_input(input_);
    }
}

void Engine::mainLoop() {
    while (!window_.shouldClose()) {
        DeltaTime::instance().tick();
        window_.pollEvents();
        input_.begin_frame();
        input_.poll();

        const float frame_dt = measure_frame_delta();

        handle_scene_switch_input();
        handle_benchmark_input(frame_dt);
        handle_stress_input();
        handle_backend_input();
        handle_camera_input(frame_dt);

        // last_frame_cpu_timings() describes the previous draw(), i.e. the same loop iteration
        // frame_dt covers, so the two pair up correctly. GPU samples lag a couple of frames and are
        // joined by serial inside Benchmark.
        (void)benchmark_.tick(frame_dt, renderer_->last_frame_cpu_timings());
        renderer_->drain_gpu_samples(gpu_sample_scratch_);
        benchmark_.apply_gpu_samples(gpu_sample_scratch_);

        if (stress_suite_active_) {
            advance_stress_suite();
        } else if (diagnostic_suite_active_) {
            advance_diagnostic_suite();
        }

        if (framebufferResized_) {
            framebufferResized_ = false;
            renderer_->notifyFramebufferResized();
        }
        renderer_->set_camera(camera_);
        if (!is_rendering_paused_) {
            renderer_->draw();
        }
    }
}

} // namespace engine
