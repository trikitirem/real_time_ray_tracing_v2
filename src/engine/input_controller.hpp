#pragma once

#include <cstdint>

struct GLFWwindow;

namespace engine {

class InputController {
public:
    void attach(GLFWwindow* window);
    void begin_frame();
    void poll();

    [[nodiscard]] bool move_forward() const { return move_forward_; }
    [[nodiscard]] bool move_backward() const { return move_backward_; }
    [[nodiscard]] bool move_left() const { return move_left_; }
    [[nodiscard]] bool move_right() const { return move_right_; }
    [[nodiscard]] bool move_up() const { return move_up_; }
    [[nodiscard]] bool move_down() const { return move_down_; }

    [[nodiscard]] float mouse_delta_x() const { return mouse_dx_; }
    [[nodiscard]] float mouse_delta_y() const { return mouse_dy_; }

private:
    GLFWwindow* window_ = nullptr;

    bool move_forward_ = false;
    bool move_backward_ = false;
    bool move_left_ = false;
    bool move_right_ = false;
    bool move_up_ = false;
    bool move_down_ = false;

    bool first_mouse_sample_ = true;
    double last_mouse_x_ = 0.0;
    double last_mouse_y_ = 0.0;
    float mouse_dx_ = 0.0f;
    float mouse_dy_ = 0.0f;
};

} // namespace engine
