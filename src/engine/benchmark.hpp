#pragma once

#include "engine/benchmark_constants.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

struct BenchmarkMeta {
    std::string scene_name;
    int         object_count    = 0;
    int         vertex_count    = 0;
    int         triangle_count  = 0;
    int         stress_count    = 0;
    int         stress_rng_seed = 0;

    std::string backend;
    float       configured_duration_s = kDefaultBenchmarkDurationS;
    std::string present_mode;

    std::string gpu_name;
    std::uint32_t window_width  = 0;
    std::uint32_t window_height = 0;

    bool rt_reflections_enabled = true;
};

struct BenchmarkStats {
    float avg_fps    = 0.0f;
    float p5_low_fps = 0.0f;
    float p1_low_fps = 0.0f;
    int   frame_count = 0;
};

class Benchmark {
public:
    void start(BenchmarkMeta meta);
    void stop();
    [[nodiscard]] bool tick(float frame_dt);
    [[nodiscard]] bool consume_finished();
    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] float elapsed_s() const { return elapsed_s_; }
    [[nodiscard]] const BenchmarkMeta& meta() const { return meta_; }
    [[nodiscard]] BenchmarkStats stats() const;

private:
    void save_json() const;

    BenchmarkMeta meta_{};
    bool          running_          = false;
    bool          just_finished_    = false;
    float         elapsed_s_          = 0.0f;
    int           warmup_remaining_   = kWarmupFrames;
    std::vector<float> frame_times_{};

    using Clock = std::chrono::steady_clock;
    Clock::time_point started_at_{};
    std::chrono::system_clock::time_point started_at_wall_{};
};

} // namespace engine
