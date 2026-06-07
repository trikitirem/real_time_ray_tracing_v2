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

    [[nodiscard]] bool pressed_f1() const { return edge_f1_; }
    [[nodiscard]] bool pressed_f2() const { return edge_f2_; }
    [[nodiscard]] bool pressed_f3() const { return edge_f3_; }
    [[nodiscard]] bool pressed_f5() const { return edge_f5_; }
    [[nodiscard]] bool pressed_f7() const { return edge_f7_; }
    [[nodiscard]] bool pressed_tab() const { return edge_tab_; }
    [[nodiscard]] bool pressed_p() const { return edge_p_; }
    [[nodiscard]] bool pressed_camera_lock_toggle() const { return edge_camera_lock_; }
    [[nodiscard]] bool pressed_stress_increase() const { return edge_stress_inc_; }
    [[nodiscard]] bool pressed_stress_decrease() const { return edge_stress_dec_; }

private:
    [[nodiscard]] bool edge_trigger(int key, bool& prev_down) const;

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

    mutable bool prev_f1_ = false;
    mutable bool prev_f2_ = false;
    mutable bool prev_f3_ = false;
    mutable bool prev_f5_ = false;
    mutable bool prev_f7_ = false;
    mutable bool prev_tab_ = false;
    mutable bool prev_p_ = false;
    mutable bool prev_camera_lock_ = false;
    mutable bool prev_stress_inc_ = false;
    mutable bool prev_stress_dec_ = false;

    bool edge_f1_ = false;
    bool edge_f2_ = false;
    bool edge_f3_ = false;
    bool edge_f5_ = false;
    bool edge_f7_ = false;
    bool edge_tab_ = false;
    bool edge_p_ = false;
    bool edge_camera_lock_ = false;
    bool edge_stress_inc_ = false;
    bool edge_stress_dec_ = false;
};

} // namespace engine
