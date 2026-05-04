#pragma once

#include "engine/window.hpp"

namespace engine {
class Engine {
public:
    Engine()                           = default;
    Engine(const Engine&)              = delete;
    Engine& operator=(const Engine&)   = delete;

    int run();

private:
    void initWindow();
    void mainLoop() const;

    Window window_;
};

} // namespace engine
