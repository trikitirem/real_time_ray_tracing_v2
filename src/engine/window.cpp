#include "engine/window.hpp"

#include <stdexcept>

#include <GLFW/glfw3.h>

namespace engine {

Window::Window() = default;

Window::~Window()
{
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwInited_) {
        glfwTerminate();
        glfwInited_ = false;
    }
}

void Window::init(const std::uint32_t width, const std::uint32_t height, const std::string& title)
{
    if (glfwInited_) {
        throw std::logic_error("Window::init called twice");
    }

    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }
    glfwInited_ = true;

    if (!glfwVulkanSupported()) {
        throw std::runtime_error("Vulkan not supported by GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    window_ = glfwCreateWindow(static_cast<int>(width),
                               static_cast<int>(height),
                               title.c_str(),
                               nullptr,
                               nullptr);
    if (!window_) {
        throw std::runtime_error("glfwCreateWindow failed");
    }

    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            const int xpos = (mode->width - static_cast<int>(width)) / 2;
            const int ypos = (mode->height - static_cast<int>(height)) / 2;
            glfwSetWindowPos(window_, xpos, ypos);
        }
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, &Window::framebufferSizeCallback);
}

bool Window::shouldClose() const
{
    return window_ != nullptr && glfwWindowShouldClose(window_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::framebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(window_, &width, &height);
}

void Window::framebufferSizeCallback(GLFWwindow* window, const int width, const int height)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr && self->resizeCb_) {
        self->resizeCb_(width, height);
    }
}

} // namespace engine
