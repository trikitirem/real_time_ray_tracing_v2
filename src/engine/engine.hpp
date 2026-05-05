#pragma once

#include <memory>

#include "engine/window.hpp"
#include "renderer/shared/device_config.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/renderer.hpp"

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
    std::unique_ptr<renderer::Renderer> renderer_;

    bool framebufferResized_ = false;
};

} // namespace engine
