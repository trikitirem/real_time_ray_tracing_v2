#pragma once

#include "engine/benchmark.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace engine {

struct SessionRunStats {
    int run_index = 0;
    float avg_fps = 0.0f;
    float p1_low_fps = 0.0f;
    int frame_count = 0;

    float avg_gpu_ms = 0.0f;
    float p1_gpu_ms = 0.0f;
    float avg_gpu_fps = 0.0f;
    float p1_low_gpu_fps = 0.0f;
    int gpu_frame_count = 0;

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

struct SessionConfigStats {
    std::vector<SessionRunStats> runs;
    float avg_fps = 0.0f;
    float p1_low_fps = 0.0f;

    // Averaged over the runs that actually produced GPU samples.
    float avg_gpu_ms = 0.0f;
    float p1_gpu_ms = 0.0f;
    float avg_gpu_fps = 0.0f;
    float p1_low_gpu_fps = 0.0f;

    float avg_fence_wait_ms = 0.0f;
    float avg_acquire_ms = 0.0f;
    float avg_record_ms = 0.0f;
    float avg_submit_ms = 0.0f;
    float avg_present_ms = 0.0f;
};

class BenchmarkSession {
  public:
    BenchmarkSession() = default;

    void init(std::string gpu_name, std::uint32_t window_width, std::uint32_t window_height);

    [[nodiscard]] bool is_initialized() const {
        return initialized_;
    }

    [[nodiscard]] const std::filesystem::path& output_path() const {
        return output_path_;
    }

    // Shared by the summary JSON and every per-frame CSV of this session.
    [[nodiscard]] const std::string& session_timestamp() const {
        return started_at_str_;
    }

    void add_run(const std::string& backend_key, int stress_count, const BenchmarkStats& stats,
                 int run_index);

    void flush() const;

  private:
    bool initialized_ = false;

    std::string gpu_name_;
    std::uint32_t window_width_ = 0;
    std::uint32_t window_height_ = 0;
    std::chrono::system_clock::time_point started_at_;
    std::string started_at_str_;

    // backend_key -> stress_count -> aggregated config stats
    std::map<std::string, std::map<int, SessionConfigStats>> results_;

    std::filesystem::path output_path_;
};

} // namespace engine
