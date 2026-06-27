#include "engine/benchmark_session.hpp"

#include "util/asset_root.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace engine {

namespace {

std::string format_timestamp(const std::chrono::system_clock::time_point tp) {
    const auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val{};
#if defined(_WIN32)
    localtime_s(&tm_val, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_val);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

} // namespace

void BenchmarkSession::init(std::string gpu_name, const std::uint32_t window_width,
                            const std::uint32_t window_height) {
    gpu_name_ = std::move(gpu_name);
    window_width_ = window_width;
    window_height_ = window_height;
    started_at_ = std::chrono::system_clock::now();
    results_.clear();

    const std::string filename =
        "StressTest_summary_" + format_timestamp(started_at_) + ".json";
    output_path_ = util::resolve_asset("measurements") / filename;

    initialized_ = true;
    std::cout << "[Session] Initialized -> measurements/" << filename << '\n';
}

void BenchmarkSession::add_run(const std::string& backend_key, const int stress_count,
                               const BenchmarkStats& stats, const int run_index) {
    SessionRunStats run{};
    run.run_index = run_index;
    run.avg_fps = stats.avg_fps;
    run.p1_low_fps = stats.p1_low_fps;
    run.frame_count = stats.frame_count;

    auto& config = results_[backend_key][stress_count];
    config.runs.push_back(run);

    const auto n = static_cast<float>(config.runs.size());
    float sum_avg = 0.0f;
    float sum_p1 = 0.0f;
    for (const auto& r : config.runs) {
        sum_avg += r.avg_fps;
        sum_p1 += r.p1_low_fps;
    }
    config.avg_fps = sum_avg / n;
    config.p1_low_fps = sum_p1 / n;
}

void BenchmarkSession::flush() const {
    if (!initialized_) {
        return;
    }

    nlohmann::json j;
    j["session_started_at"] = format_timestamp(started_at_);
    j["gpu_name"] = gpu_name_;
    j["window_width"] = window_width_;
    j["window_height"] = window_height_;

    for (const auto& [backend_key, configs] : results_) {
        nlohmann::json backend_obj = nlohmann::json::object();
        for (const auto& [stress_count, config] : configs) {
            nlohmann::json config_obj;
            nlohmann::json runs_arr = nlohmann::json::array();
            for (const auto& run : config.runs) {
                runs_arr.push_back({{"run", run.run_index + 1},
                                    {"avg_fps", run.avg_fps},
                                    {"p1_low_fps", run.p1_low_fps},
                                    {"frame_count", run.frame_count}});
            }
            config_obj["runs"] = std::move(runs_arr);
            config_obj["avg_fps"] = config.avg_fps;
            config_obj["p1_low_fps"] = config.p1_low_fps;
            backend_obj[std::to_string(stress_count)] = std::move(config_obj);
        }
        j[backend_key] = std::move(backend_obj);
    }

    std::ofstream out(output_path_);
    if (!out) {
        std::cerr << "[Session] Failed to write: " << output_path_.string() << '\n';
        return;
    }
    out << std::setw(2) << j << '\n';
    std::cout << "[Session] Flushed -> " << output_path_.filename().string() << '\n';
}

} // namespace engine
