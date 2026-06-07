#include "engine/benchmark.hpp"

#include "util/asset_root.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace engine {

namespace {

std::string sanitize_filename_part(std::string value)
{
    for (char& c : value) {
        if (c == ' ') {
            c = '_';
        } else if (c == ':' || c == '/' || c == '\\') {
            c = '-';
        }
    }
    return value;
}

std::string format_timestamp(const std::chrono::system_clock::time_point tp)
{
    const auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm    tm_val{};
#if defined(_WIN32)
    localtime_s(&tm_val, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_val);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

float percentile_low_fps(const std::vector<float>& sorted_frame_times, const float percentile)
{
    if (sorted_frame_times.empty()) {
        return 0.0f;
    }
    const std::size_t count
        = std::max<std::size_t>(1, static_cast<std::size_t>(
                                        std::ceil(sorted_frame_times.size() * percentile)));
    float sum_fps = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float dt = sorted_frame_times[i];
        sum_fps += dt > 0.0f ? 1.0f / dt : 0.0f;
    }
    return sum_fps / static_cast<float>(count);
}

} // namespace

void Benchmark::start(BenchmarkMeta meta)
{
    meta_               = std::move(meta);
    running_            = true;
    elapsed_s_          = 0.0f;
    warmup_remaining_   = kWarmupFrames;
    frame_times_.clear();
    started_at_      = Clock::now();
    started_at_wall_ = std::chrono::system_clock::now();
}

void Benchmark::stop()
{
    if (!running_) {
        return;
    }
    running_ = false;

    const auto stats = this->stats();
    save_json();

    std::cout << "[Benchmark] avg_fps=" << stats.avg_fps << "  p5_low=" << stats.p5_low_fps
              << "  frames=" << stats.frame_count << '\n';
}

bool Benchmark::tick(const float frame_dt)
{
    if (!running_) {
        return false;
    }

    elapsed_s_ += frame_dt;

    if (warmup_remaining_ > 0) {
        --warmup_remaining_;
    } else {
        frame_times_.push_back(frame_dt);
    }

    if (elapsed_s_ >= meta_.configured_duration_s) {
        std::cout << "[Benchmark] Finished (" << meta_.configured_duration_s << "s)\n";
        stop();
        return false;
    }
    return true;
}

BenchmarkStats Benchmark::stats() const
{
    BenchmarkStats result{};
    result.frame_count = static_cast<int>(frame_times_.size());
    if (frame_times_.empty()) {
        return result;
    }

    auto sorted = frame_times_;
    std::sort(sorted.begin(), sorted.end());

    float sum_fps = 0.0f;
    for (const float dt : frame_times_) {
        sum_fps += dt > 0.0f ? 1.0f / dt : 0.0f;
    }
    result.avg_fps    = sum_fps / static_cast<float>(frame_times_.size());
    result.p5_low_fps = percentile_low_fps(sorted, 0.05f);
    result.p1_low_fps = percentile_low_fps(sorted, 0.01f);
    return result;
}

void Benchmark::save_json() const
{
    const auto stats = this->stats();

    nlohmann::json j;
    j["scene_name"]              = meta_.scene_name;
    j["backend"]                 = meta_.backend;
    j["gpu_name"]                = meta_.gpu_name;
    j["window_width"]            = meta_.window_width;
    j["window_height"]           = meta_.window_height;
    j["present_mode"]            = meta_.present_mode;
    j["configured_duration_s"]   = meta_.configured_duration_s;
    j["elapsed_s"]               = elapsed_s_;
    j["warmup_frames_discarded"] = kWarmupFrames;
    j["object_count"]            = meta_.object_count;
    j["vertex_count"]            = meta_.vertex_count;
    j["triangle_count"]          = meta_.triangle_count;
    j["stress_count"]            = meta_.stress_count;
    j["stress_rng_seed"]         = meta_.stress_rng_seed;
    j["stats"]["avg_fps"]        = stats.avg_fps;
    j["stats"]["p5_low_fps"]     = stats.p5_low_fps;
    j["stats"]["p1_low_fps"]     = stats.p1_low_fps;
    j["stats"]["frame_count"]    = stats.frame_count;

    nlohmann::json frames = nlohmann::json::array();
    for (const float dt : frame_times_) {
        frames.push_back({ { "dt_s", dt }, { "fps", dt > 0.0f ? 1.0f / dt : 0.0f } });
    }
    j["frames"] = std::move(frames);

    const std::string filename = sanitize_filename_part(meta_.scene_name) + "_"
                                 + meta_.backend + "_"
                                 + format_timestamp(started_at_wall_) + ".json";
    const auto out_path = util::resolve_asset("measurements") / filename;

    std::ofstream out(out_path);
    if (!out) {
        std::cerr << "[Benchmark] Failed to write: " << out_path.string() << '\n';
        return;
    }
    out << std::setw(2) << j << '\n';
    std::cout << "[Benchmark] Saved -> measurements/" << filename << '\n';
}

} // namespace engine
