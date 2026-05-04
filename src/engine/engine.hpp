#pragma once

#include "engine/window.hpp"
#include "renderer/shared/device_config.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/swapchain.hpp"

namespace engine {

class Engine {
public:
    explicit Engine(bool useRasterBackend);
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    int run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();

    const bool             useRasterBackend_;
    renderer::DeviceConfig rasterConfig_;
    renderer::DeviceConfig rayTracingConfig_;

    Window                  window_;
    renderer::DeviceContext deviceContext_;
    renderer::Swapchain     swapchain_;

    bool framebufferResized_ = false;
};

} // namespace engine
