#pragma once

#include "engine/benchmark_constants.hpp"
#include "renderer/shared/frame_timings.hpp"

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

struct BenchmarkMeta {
    std::string scene_name;
    int object_count = 0;
    int vertex_count = 0;
    int triangle_count = 0;
    int stress_count = 0;
    int stress_rng_seed = 0;

    std::string backend;
    float configured_duration_s = kDefaultBenchmarkDurationS;
    std::string present_mode;

    std::string gpu_name;
    std::uint32_t window_width = 0;
    std::uint32_t window_height = 0;

    bool rt_reflections_enabled = true;
    bool stress_use_texture = false;
};

struct BenchmarkStats {
    float avg_fps = 0.0f;
    float p1_low_fps = 0.0f;
    int frame_count = 0;

    // GPU command buffer execution time, measured with timestamp queries. Expressed both in ms and
    // as reciprocals, so avg_gpu_fps / p1_low_gpu_fps use the exact same formula as their
    // wall-clock counterparts above and the two S1% ratios stay comparable.
    float avg_gpu_ms = 0.0f;
    float p1_gpu_ms = 0.0f;
    float avg_gpu_fps = 0.0f;
    float p1_low_gpu_fps = 0.0f;
    int gpu_frame_count = 0;

    // Per blocking CPU stage, in ms: mean, and mean of the worst 1% (same "worst 1%" convention as
    // p1_low_fps / p1_gpu_ms above).
    float avg_fence_wait_ms = 0.0f;
    float avg_acquire_ms = 0.0f;
    float avg_record_ms = 0.0f;
    float avg_submit_ms = 0.0f;
    float avg_present_ms = 0.0f;
    float p1_high_fence_wait_ms = 0.0f;
    float p1_high_acquire_ms = 0.0f;
    float p1_high_record_ms = 0.0f;
    float p1_high_submit_ms = 0.0f;
    float p1_high_present_ms = 0.0f;
};

// One measured frame, kept only when a frame trace was requested (diagnostic runs).
struct FrameSample {
    renderer::FrameSerial serial = renderer::kInvalidFrameSerial;
    float wall_ms = 0.0f;
    float gpu_ms = 0.0f;        // NaN until the GPU sample for this serial arrives
    float fence_wait_ms = 0.0f; // NaN when draw() did not submit this iteration
    float acquire_ms = 0.0f;
    float record_ms = 0.0f;
    float submit_ms = 0.0f;
    float present_ms = 0.0f;
};

class Benchmark {
  public:
    // collect_frame_trace enables per-frame retention (level 2). Aggregate GPU stats are collected
    // either way, at the cost of one float per frame.
    void start(BenchmarkMeta meta, bool collect_frame_trace = false);
    void stop();
    [[nodiscard]] bool tick(float frame_dt, const renderer::FrameCpuTimings& cpu_timings);
    void apply_gpu_samples(std::span<const renderer::GpuTimeSample> samples);
    [[nodiscard]] bool consume_finished();
    [[nodiscard]] bool is_running() const {
        return running_;
    }
    [[nodiscard]] float elapsed_s() const {
        return elapsed_s_;
    }
    [[nodiscard]] const BenchmarkMeta& meta() const {
        return meta_;
    }
    [[nodiscard]] BenchmarkStats stats() const;

    [[nodiscard]] const std::vector<FrameSample>& frame_samples() const {
        return samples_;
    }

  private:
    BenchmarkMeta meta_{};
    bool running_ = false;
    bool just_finished_ = false;
    float elapsed_s_ = 0.0f;
    bool warmup_complete_logged_ = false;
    std::vector<float> frame_times_{};

    // Level 1: always collected. Unordered on purpose — mean and 1% low do not depend on order,
    // so GPU samples can be appended as they resolve without joining them to a frame.
    std::vector<float> gpu_times_{};

    // Level 2: diagnostic runs only. Both stay empty and unallocated otherwise.
    bool collect_frame_trace_ = false;
    std::vector<FrameSample> samples_{};
    std::unordered_map<renderer::FrameSerial, std::size_t> serial_to_row_{};
    // Guards against attributing one draw()'s stage costs to two frames, which happens whenever
    // rendering is skipped for an iteration and Renderer keeps reporting the previous result.
    renderer::FrameSerial last_trace_serial_ = renderer::kInvalidFrameSerial;

    using Clock = std::chrono::steady_clock;
    Clock::time_point started_at_{};
};

} // namespace engine
