#pragma once

#include <algorithm>
#include <chrono>

namespace engine {

class DeltaTime {
public:
    static DeltaTime& instance()
    {
        static DeltaTime instance{};
        return instance;
    }

    DeltaTime(const DeltaTime&) = delete;
    DeltaTime& operator=(const DeltaTime&) = delete;

    void tick()
    {
        const auto now = clock::now();
        const float raw_delta
            = std::chrono::duration<float>(now - last_time_).count();
        // Clamp long stalls (alt-tab, breakpoints) so movement remains stable.
        delta_seconds_ = std::clamp(raw_delta, 0.0f, max_delta_seconds_);
        last_time_ = now;
    }

    [[nodiscard]] float seconds() const { return delta_seconds_; }

    void set_max_delta_seconds(float max_delta_seconds)
    {
        max_delta_seconds_ = std::max(0.0f, max_delta_seconds);
    }

private:
    using clock = std::chrono::steady_clock;

    DeltaTime()
        : last_time_(clock::now())
    {
    }

    clock::time_point last_time_{};
    float delta_seconds_ = 0.0f;
    float max_delta_seconds_ = 0.1f;
};

} // namespace engine
