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

    int fbW = 0;
    int fbH = 0;
    window_.framebufferSize(fbW, fbH);
    swapchain_.create(deviceContext_, fbW, fbH);
}

void Engine::mainLoop()
{
    while (!window_.shouldClose()) {
        if (framebufferResized_) {
            framebufferResized_    = false;
            int w = 0;
            int h = 0;
            window_.framebufferSize(w, h);
            if (w > 0 && h > 0) {
                swapchain_.recreate(deviceContext_, window_.handle());
            }
        }
        window_.pollEvents();
    }
}

} // namespace engine
