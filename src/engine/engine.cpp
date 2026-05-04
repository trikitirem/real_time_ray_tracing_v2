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
}

void Engine::initVulkan()
{
    const renderer::DeviceConfig& cfg =
        useRasterBackend_ ? rasterConfig_ : rayTracingConfig_;
    deviceContext_.init(window_.handle(), cfg);
}

void Engine::mainLoop() const
{
    while (!window_.shouldClose()) {
        window_.pollEvents();
    }
}

} // namespace engine
