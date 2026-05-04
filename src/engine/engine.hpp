#pragma once

#include "engine/window.hpp"
#include "renderer/shared/device_context.hpp"

namespace engine {

class Engine {
public:
    Engine()                         = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    int run();

private:
    void initWindow();
    void initRenderer();
    void mainLoop() const;

    Window                  window_;
    renderer::DeviceContext deviceContext_;
};

} // namespace engine
