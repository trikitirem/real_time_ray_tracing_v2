#include "engine/benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace engine {

namespace {

float percentile_low_fps(const std::vector<float>& sorted_frame_times, const float percentile) {
    if (sorted_frame_times.empty()) {
        return 0.0f;
    }
    const std::size_t count = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(sorted_frame_times.size() * percentile)));
    // Take from the END of the ascending-sorted array: longest dt = slowest frames = lowest FPS.
    const std::size_t start = sorted_frame_times.size() - count;
    float sum_fps = 0.0f;
    for (std::size_t i = start; i < sorted_frame_times.size(); ++i) {
        const float dt = sorted_frame_times[i];
        sum_fps += dt > 0.0f ? 1.0f / dt : 0.0f;
    }
    return sum_fps / static_cast<float>(count);
}

} // namespace

void Benchmark::start(BenchmarkMeta meta) {
    meta_ = std::move(meta);
    running_ = true;
    elapsed_s_ = 0.0f;
    warmup_complete_logged_ = false;
    frame_times_.clear();
    started_at_ = Clock::now();
}

void Benchmark::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    just_finished_ = true;

    const auto s = this->stats();
    std::cout << "[Benchmark] avg_fps=" << s.avg_fps << "  p1_low=" << s.p1_low_fps
              << "  frames=" << s.frame_count << '\n';
}

bool Benchmark::consume_finished() {
    if (just_finished_) {
        just_finished_ = false;
        return true;
    }
    return false;
}

bool Benchmark::tick(const float frame_dt) {
    if (!running_) {
        return false;
    }

    elapsed_s_ += frame_dt;

    if (elapsed_s_ > kWarmupDurationS) {
        frame_times_.push_back(frame_dt);
        if (!warmup_complete_logged_) {
            warmup_complete_logged_ = true;
            std::cout << "[Benchmark] Warmup complete (" << kWarmupDurationS << "s)\n";
        }
    }

    const float total_duration_s = kWarmupDurationS + meta_.configured_duration_s;
    if (elapsed_s_ >= total_duration_s) {
        std::cout << "[Benchmark] Finished (" << meta_.configured_duration_s << "s measured, "
                  << kWarmupDurationS << "s warmup)\n";
        stop();
        return false;
    }
    return true;
}

BenchmarkStats Benchmark::stats() const {
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
    result.avg_fps = sum_fps / static_cast<float>(frame_times_.size());
    result.p1_low_fps = percentile_low_fps(sorted, 0.01f);
    return result;
}

} // namespace engine
