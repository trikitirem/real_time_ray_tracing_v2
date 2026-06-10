#include "engine/engine.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

#include "engine/delta_time.hpp"
#include "renderer/raster/device_config.hpp"
#include "renderer/ray_tracing/device_config.hpp"
#include "util/asset_root.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace engine {

namespace {

constexpr std::uint32_t kInitialWidth  = 1920;
constexpr std::uint32_t kInitialHeight = 1080;

glm::quat euler_degrees_to_quat(const glm::vec3& euler_deg)
{
    const glm::vec3 rad = glm::radians(euler_deg);
    const glm::quat yaw_rot   = glm::angleAxis(rad.y, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitch_rot = glm::angleAxis(rad.x, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat roll_rot  = glm::angleAxis(rad.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::normalize(yaw_rot * pitch_rot * roll_rot);
}

float elapsed_seconds_since(const std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

const char* backend_name(const bool is_raster)
{
    return is_raster ? "raster" : "rt";
}

} // namespace

Engine::Engine(const bool useRasterBackend)
    : useRasterBackendFromMain_(useRasterBackend)
    , rasterConfig_(renderer::raster::makeRasterDeviceConfig())
    , rayTracingConfig_(renderer::ray_tracing::makeRayTracingDeviceConfig())
{
    load_scene_content(scene::SceneName::Test);
}

int Engine::run()
{
    initWindow();
    initVulkan();
    log_startup_info();
    mainLoop();
    return 0;
}

void Engine::load_scene_content(const scene::SceneName name)
{
    current_scene_name_ = name;
    scene_              = scene::Scene{};

    const auto json_path      = util::resolve_asset(scene::scene_json_path(name));
    current_scene_config_     = scene::load_scene_config(json_path);
    if (name == scene::SceneName::StressTest && current_scene_config_.stress.enabled) {
        current_stress_count_ = current_scene_config_.stress.initial_count;
    } else {
        current_stress_count_ = 0;
    }

    auto [built_scene, stats] = scene::build_scene(
        current_scene_config_,
        util::asset_root(),
        name == scene::SceneName::StressTest ? current_stress_count_ : -1);
    scene_       = std::move(built_scene);
    scene_stats_ = stats;

    camera_presets_        = current_scene_config_.camera_presets;
    current_camera_preset_ = -1;

    if (current_scene_config_.initial_camera.has_value()) {
        const auto& cam   = *current_scene_config_.initial_camera;
        camera_.position    = cam.position;
        camera_.orientation = euler_degrees_to_quat(cam.euler_degrees);
        camera_.reset_pitch_state();
    }
}

void Engine::initWindow()
{
    window_.init(kInitialWidth, kInitialHeight, "real_time_ray_tracing_v2");
    window_.setResizeCallback(
        [this](int /*w*/, int /*h*/) { framebufferResized_ = true; });
    input_.attach(window_.handle());
}

void Engine::initVulkan()
{
    rt_extensions_supported_ = renderer::probeRayTracingSupport(window_.handle());
    backend_toggle_enabled_  = rt_extensions_supported_;

    const renderer::DeviceConfig& cfg = rt_extensions_supported_
                                            ? rayTracingConfig_
                                            : rasterConfig_;

    if (!rt_extensions_supported_ && !useRasterBackendFromMain_) {
        std::cerr << "[Engine] Ray tracing requested but not available — falling back to "
                     "raster.\n";
        active_backend_is_raster_ = true;
    } else {
        active_backend_is_raster_ = useRasterBackendFromMain_;
    }

    deviceContext_.init(window_.handle(), cfg);

    renderer_ = std::make_unique<renderer::Renderer>(
        window_.handle(), deviceContext_, active_backend_is_raster_);
    reload_scene_gpu();
    renderer_->set_camera(camera_);
}

void Engine::log_startup_info() const
{
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
}

void Engine::reload_scene_gpu()
{
    is_rendering_paused_ = true;
    std::cout << "[Engine] Pausing render, waiting for GPU...\n";
    deviceContext_.device().waitIdle();
    renderer_->set_shadow_half_extent(
        scene::compute_shadow_half_extent(current_scene_config_));
    renderer_->load_scene(scene_);
    is_rendering_paused_ = false;
}

void Engine::load_scene(const scene::SceneName name)
{
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

void Engine::apply_camera_preset(const scene::CameraPreset& preset)
{
    camera_.position    = preset.position;
    camera_.orientation = euler_degrees_to_quat(preset.euler_degrees);
    camera_.reset_pitch_state();
}

void Engine::rebuild_stress_scene(const int count)
{
    std::cout << "[Stress] Rebuilding scene: " << current_stress_count_ << " -> " << count
              << " objects...\n";
    const auto t0 = Clock::now();

    current_stress_count_ = count;
    scene_                = scene::Scene{};
    auto [built_scene, stats]
        = scene::build_scene(current_scene_config_, util::asset_root(), current_stress_count_);
    scene_       = std::move(built_scene);
    scene_stats_ = stats;
    reload_scene_gpu();

    const float elapsed_s = elapsed_seconds_since(t0);
    std::cout << "[Stress] Done: " << current_stress_count_ << " objects, "
              << scene_stats_.vertex_count << " vertices (" << elapsed_s << "s)\n";
}

void Engine::adjust_stress_count(const int delta)
{
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

void Engine::start_stress_suite()
{
    const auto& stress = current_scene_config_.stress;
    std::cout << "[Suite] Starting stress suite: " << stress.initial_count << " to "
              << stress.max_count << " step " << stress.step << '\n';

    rebuild_stress_scene(stress.initial_count);

    BenchmarkMeta meta = make_benchmark_meta();
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta);

    if (current_scene_config_.benchmark_path.has_value()) {
        animator_.start(*current_scene_config_.benchmark_path);
        camera_.reset_pitch_state();
    }

    stress_suite_active_ = true;
    std::cout << "[Suite] Run 1/" << ((stress.max_count - stress.initial_count) / stress.step + 1)
              << ": " << current_stress_count_ << " objects\n";
}

void Engine::advance_stress_suite()
{
    if (!benchmark_.consume_finished()) {
        return;
    }

    const auto& stress = current_scene_config_.stress;
    const int next     = current_stress_count_ + stress.step;

    if (next > stress.max_count) {
        stress_suite_active_ = false;
        animator_.stop();
        std::cout << "[Suite] All runs complete.\n";
        return;
    }

    rebuild_stress_scene(next);

    BenchmarkMeta meta = make_benchmark_meta();
    renderer_->set_rt_reflections_enabled(meta.rt_reflections_enabled);
    benchmark_.start(meta);

    std::cout << "[Suite] Next run: " << current_stress_count_ << " objects\n";
}

void Engine::toggle_backend()
{
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

BenchmarkMeta Engine::make_benchmark_meta() const
{
    BenchmarkMeta meta{};
    meta.scene_name            = std::string(scene::scene_display_name(current_scene_name_));
    meta.object_count          = scene_stats_.object_count;
    meta.vertex_count          = scene_stats_.vertex_count;
    meta.triangle_count        = scene_stats_.triangle_count;
    meta.stress_count          = scene_stats_.stress_count;
    meta.stress_rng_seed       = current_scene_config_.stress.rng_seed;
    meta.backend               = backend_name(active_backend_is_raster_);
    meta.configured_duration_s    = current_scene_config_.benchmark.duration_seconds;
    meta.present_mode             = renderer_->present_mode_string();
    meta.gpu_name                 = std::string(
        deviceContext_.physicalDevice().getProperties().deviceName.data());
    const auto extent             = renderer_->swapchain_extent();
    meta.window_width             = extent.width;
    meta.window_height            = extent.height;
    meta.rt_reflections_enabled   = current_scene_config_.benchmark.rt_reflections_enabled;
    return meta;
}

float Engine::measure_frame_delta()
{
    const auto now  = Clock::now();
    const float dt  = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;
    return dt;
}

void Engine::handle_scene_switch_input()
{
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

void Engine::handle_benchmark_input(const float /*frame_dt*/)
{
    if (!input_.pressed_f5()) {
        return;
    }

    if (benchmark_.is_running() || stress_suite_active_) {
        std::cout << "[Benchmark] Stopped early (" << benchmark_.elapsed_s() << "s / "
                  << benchmark_.meta().configured_duration_s << "s)\n";
        benchmark_.stop();
        animator_.stop();
        stress_suite_active_ = false;
        return;
    }

    if (is_rendering_paused_) {
        std::cout << "[Benchmark] Cannot start — scene reload in progress.\n";
        return;
    }

    if (glfwGetWindowAttrib(window_.handle(), GLFW_FOCUSED) == 0) {
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

void Engine::handle_stress_input()
{
    if (input_.pressed_stress_increase()) {
        adjust_stress_count(current_scene_config_.stress.step);
    } else if (input_.pressed_stress_decrease()) {
        adjust_stress_count(-current_scene_config_.stress.step);
    }
}

void Engine::handle_backend_input()
{
    if (input_.pressed_f7()) {
        toggle_backend();
    }
}

void Engine::handle_camera_input(const float frame_dt)
{
    if (input_.pressed_camera_lock_toggle()) {
        camera_movement_locked_ = !camera_movement_locked_;
        std::cout << "[Camera] Movement "
                  << (camera_movement_locked_ ? "locked" : "unlocked") << '\n';
    }

    if (input_.pressed_tab()) {
        if (camera_presets_.empty()) {
            std::cout << "[Camera] No camera presets on this scene.\n";
        } else {
            current_camera_preset_
                = (current_camera_preset_ + 1) % static_cast<int>(camera_presets_.size());
            apply_camera_preset(camera_presets_[static_cast<std::size_t>(current_camera_preset_)]);
            std::cout << "[Camera] Preset: " << camera_presets_[current_camera_preset_].name
                      << " (" << (current_camera_preset_ + 1) << "/"
                      << camera_presets_.size() << ")\n";
        }
    }

    if (input_.pressed_p()) {
        const glm::vec3 euler = glm::degrees(glm::eulerAngles(camera_.orientation));
        std::cout << "[Camera] position: [" << camera_.position.x << ", "
                  << camera_.position.y << ", " << camera_.position.z
                  << "]  euler_degrees: [" << euler.x << ", " << euler.y << ", " << euler.z
                  << "]  (pitch, yaw, roll — paste into JSON as-is)\n";
    }

    if (animator_.is_running()) {
        animator_.update(camera_, frame_dt);
    } else if (!benchmark_.is_running() && !camera_movement_locked_) {
        camera_.update_from_input(input_);
    }
}

void Engine::mainLoop()
{
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

        (void)benchmark_.tick(frame_dt);

        if (stress_suite_active_) {
            advance_stress_suite();
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
