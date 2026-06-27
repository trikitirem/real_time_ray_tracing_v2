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
};

struct SessionConfigStats {
    std::vector<SessionRunStats> runs;
    float avg_fps = 0.0f;
    float p1_low_fps = 0.0f;
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

    void add_run(const std::string& backend_key, int stress_count, const BenchmarkStats& stats,
                 int run_index);

    void flush() const;

  private:
    bool initialized_ = false;

    std::string gpu_name_;
    std::uint32_t window_width_ = 0;
    std::uint32_t window_height_ = 0;
    std::chrono::system_clock::time_point started_at_;

    // backend_key -> stress_count -> aggregated config stats
    std::map<std::string, std::map<int, SessionConfigStats>> results_;

    std::filesystem::path output_path_;
};

} // namespace engine
