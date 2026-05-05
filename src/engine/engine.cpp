#include "engine/engine.hpp"

#include <cstdint>

#include "renderer/raster/device_config.hpp"
#include "renderer/ray_tracing/device_config.hpp"

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
}

int Engine::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    return 0;
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
