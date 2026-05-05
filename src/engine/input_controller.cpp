#include "engine/input_controller.hpp"

#include <stdexcept>

#include <GLFW/glfw3.h>

namespace engine {

namespace {

bool key_down(GLFWwindow* window, int key)
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}

} // namespace

void InputController::attach(GLFWwindow* window)
{
    if (window == nullptr) {
        throw std::runtime_error("InputController::attach requires valid window");
    }
    window_ = window;
    first_mouse_sample_ = true;
    mouse_dx_ = 0.0f;
    mouse_dy_ = 0.0f;
}

void InputController::begin_frame()
{
    mouse_dx_ = 0.0f;
    mouse_dy_ = 0.0f;
}

void InputController::poll()
{
    if (window_ == nullptr) {
        return;
    }

    move_forward_ = key_down(window_, GLFW_KEY_W);
    move_backward_ = key_down(window_, GLFW_KEY_S);
    move_left_ = key_down(window_, GLFW_KEY_A);
    move_right_ = key_down(window_, GLFW_KEY_D);
    move_up_ = key_down(window_, GLFW_KEY_SPACE);
    move_down_ = key_down(window_, GLFW_KEY_LEFT_CONTROL);

    double mouse_x = 0.0;
    double mouse_y = 0.0;
    glfwGetCursorPos(window_, &mouse_x, &mouse_y);
    if (first_mouse_sample_) {
        first_mouse_sample_ = false;
    } else {
        mouse_dx_ = static_cast<float>(mouse_x - last_mouse_x_);
        mouse_dy_ = static_cast<float>(mouse_y - last_mouse_y_);
    }
    last_mouse_x_ = mouse_x;
    last_mouse_y_ = mouse_y;
}

} // namespace engine
