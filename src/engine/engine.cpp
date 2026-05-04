#include "engine/engine.hpp"

#include <cstdint>

#include "renderer/raster/device_config.hpp"

namespace engine {

namespace {

constexpr std::uint32_t kInitialWidth  = 1280;
constexpr std::uint32_t kInitialHeight = 720;

} // namespace

int Engine::run()
{
    initWindow();
    initRenderer();
    mainLoop();
    return 0;
}

void Engine::initWindow()
{
    window_.init(kInitialWidth, kInitialHeight, "real_time_ray_tracing_v2");
}

void Engine::initRenderer()
{
    deviceContext_.init(window_.handle(), renderer::raster::makeRasterDeviceConfig());
}

void Engine::mainLoop() const {
    while (!window_.shouldClose()) {
        window_.pollEvents();
    }
}

} // namespace engine
