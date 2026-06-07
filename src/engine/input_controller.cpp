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
    edge_f1_ = edge_f2_ = edge_f3_ = false;
    edge_f5_ = edge_f7_ = edge_tab_ = edge_p_ = false;
    edge_camera_lock_ = false;
    edge_stress_inc_ = edge_stress_dec_ = false;
}

bool InputController::edge_trigger(const int key, bool& prev_down) const
{
    const bool down = key_down(window_, key);
    const bool edge = down && !prev_down;
    prev_down       = down;
    return edge;
}

void InputController::poll()
{
    if (window_ == nullptr) {
        return;
    }

    if (glfwGetWindowAttrib(window_, GLFW_FOCUSED) == 0) {
        move_forward_ = false;
        move_backward_ = false;
        move_left_ = false;
        move_right_ = false;
        move_up_ = false;
        move_down_ = false;
        first_mouse_sample_ = true;
        mouse_dx_ = 0.0f;
        mouse_dy_ = 0.0f;
        prev_f1_ = prev_f2_ = prev_f3_ = false;
        prev_f5_ = prev_f7_ = prev_tab_ = prev_p_ = false;
        prev_camera_lock_ = false;
        prev_stress_inc_ = prev_stress_dec_ = false;
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

    edge_f1_          = edge_trigger(GLFW_KEY_F1, prev_f1_);
    edge_f2_          = edge_trigger(GLFW_KEY_F2, prev_f2_);
    edge_f3_          = edge_trigger(GLFW_KEY_F3, prev_f3_);
    edge_f5_          = edge_trigger(GLFW_KEY_F5, prev_f5_);
    edge_f7_          = edge_trigger(GLFW_KEY_F7, prev_f7_);
    edge_tab_         = edge_trigger(GLFW_KEY_TAB, prev_tab_);
    edge_p_           = edge_trigger(GLFW_KEY_P, prev_p_);
    edge_camera_lock_ = edge_trigger(GLFW_KEY_L, prev_camera_lock_);
    edge_stress_inc_  = edge_trigger(GLFW_KEY_RIGHT_BRACKET, prev_stress_inc_);
    edge_stress_dec_  = edge_trigger(GLFW_KEY_LEFT_BRACKET, prev_stress_dec_);
}

} // namespace engine
