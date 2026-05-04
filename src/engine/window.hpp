#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct GLFWwindow;

namespace engine {

class Window {
public:
    using ResizeCallback = std::function<void(int width, int height)>;

    Window();
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    void init(std::uint32_t width, std::uint32_t height, const std::string& title);

    [[nodiscard]] GLFWwindow* handle() const { return window_; }
    [[nodiscard]] bool        shouldClose() const;

    static void        pollEvents();

    void setResizeCallback(ResizeCallback callback) { resizeCb_ = std::move(callback); }

    void framebufferSize(int& width, int& height) const;

private:
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow*    window_       = nullptr;
    bool           glfwInited_ = false;
    ResizeCallback resizeCb_;
};

} // namespace engine
