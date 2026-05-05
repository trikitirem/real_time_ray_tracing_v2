#include "engine/engine.hpp"

#include <cstdint>
#include <filesystem>

#include "renderer/raster/device_config.hpp"
#include "renderer/ray_tracing/device_config.hpp"
#include "scene/primitives/cube.hpp"
#include "scene/primitives/plane.hpp"
#include "util/asset_location.hpp"

namespace engine {

namespace {

constexpr std::uint32_t kInitialWidth  = 1280;
constexpr std::uint32_t kInitialHeight = 720;

} // namespace

Engine::Engine(bool useRasterBackend)
    : useRasterBackend_(useRasterBackend)
    , rasterConfig_(renderer::raster::makeRasterDeviceConfig())
    , rayTracingConfig_(renderer::ray_tracing::makeRayTracingDeviceConfig())
{
    initTestScene();
}

int Engine::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    return 0;
}

void Engine::initTestScene()
{
    scene::Material cube_mat{};
    cube_mat.texture_location
        = util::AssetLocation{ std::filesystem::path("images"), "textue.jpg" };
    cube_mat.base_color = glm::vec3(1.0f);
    cube_mat.metalness  = 0.0f;
    cube_mat.roughness  = 0.7f;

    scene::Transform cube_xf{};
    cube_xf.translate(0.0f, 0.5f, 0.0f);
    scene_.add_model(
        scene::primitives::make_cube(1.0f, 1.0f, 1.0f, cube_mat, cube_xf));

    scene::Material floor_mat{};
    floor_mat.base_color = glm::vec3(0.12f, 0.12f, 0.12f);
    floor_mat.metalness  = 1.0f;
    floor_mat.roughness  = 0.05f;

    scene::Transform floor_xf{};
    floor_xf.translate(0.0f, -0.001f, 0.0f);
    scene_.add_model(scene::primitives::make_plane(10.0f, 10.0f, floor_mat, floor_xf));

    camera_.position = glm::vec3(0.0f, 1.5f, 4.0f);
}

void Engine::initWindow()
{
    window_.init(kInitialWidth, kInitialHeight, "real_time_ray_tracing_v2");
    window_.setResizeCallback(
        [this](int /*w*/, int /*h*/) { framebufferResized_ = true; });
}

void Engine::initVulkan()
{
    const renderer::DeviceConfig& cfg =
        useRasterBackend_ ? rasterConfig_ : rayTracingConfig_;
    deviceContext_.init(window_.handle(), cfg);

    renderer_
        = std::make_unique<renderer::Renderer>(window_.handle(), deviceContext_, useRasterBackend_);
    renderer_->load_scene(scene_);
}

void Engine::mainLoop()
{
    while (!window_.shouldClose()) {
        if (framebufferResized_) {
            framebufferResized_ = false;
            renderer_->notifyFramebufferResized();
        }
        renderer_->draw();
        window_.pollEvents();
    }
}

} // namespace engine
