#include "engine/benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

namespace engine {

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

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

// Mean of the slowest `percentile` fraction of an ascending-sorted duration list.
float percentile_high_ms(const std::vector<float>& sorted_ms, const float percentile) {
    if (sorted_ms.empty()) {
        return 0.0f;
    }
    const std::size_t count = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(sorted_ms.size() * percentile)));
    const std::size_t start = sorted_ms.size() - count;
    const float sum = std::accumulate(sorted_ms.begin() + static_cast<std::ptrdiff_t>(start),
                                      sorted_ms.end(), 0.0f);
    return sum / static_cast<float>(count);
}

float mean_of(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.0f;
    }
    return std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(values.size());
}

// Reduces one CPU stage across all samples, skipping frames that did not submit (NaN).
void stage_stats(const std::vector<FrameSample>& samples, float FrameSample::* member, float& avg,
                 float& p1_high) {
    std::vector<float> values;
    values.reserve(samples.size());
    for (const auto& s : samples) {
        const float v = s.*member;
        if (!std::isnan(v)) {
            values.push_back(v);
        }
    }
    if (values.empty()) {
        avg = 0.0f;
        p1_high = 0.0f;
        return;
    }
    avg = mean_of(values);
    std::sort(values.begin(), values.end());
    p1_high = percentile_high_ms(values, 0.01f);
}

} // namespace

void Benchmark::start(BenchmarkMeta meta, const bool collect_frame_trace) {
    meta_ = std::move(meta);
    running_ = true;
    elapsed_s_ = 0.0f;
    warmup_complete_logged_ = false;
    frame_times_.clear();
    gpu_times_.clear();
    samples_.clear();
    serial_to_row_.clear();
    last_trace_serial_ = renderer::kInvalidFrameSerial;
    collect_frame_trace_ = collect_frame_trace;

    if (collect_frame_trace_) {
        // Reserve up front: a vector reallocation mid-run would itself show up as a frame spike and
        // corrupt the very metric this measures. 6000 fps is a generous headroom over the fastest
        // observed configuration (~4600 fps).
        const auto estimate =
            static_cast<std::size_t>(meta_.configured_duration_s * 6000.0f) + 1024;
        samples_.reserve(estimate);
        serial_to_row_.reserve(estimate);
    }

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
    if (s.gpu_frame_count > 0) {
        const float s1_wall = s.avg_fps > 0.0f ? s.p1_low_fps / s.avg_fps * 100.0f : 0.0f;
        const float s1_gpu =
            s.avg_gpu_fps > 0.0f ? s.p1_low_gpu_fps / s.avg_gpu_fps * 100.0f : 0.0f;
        std::cout << "[Benchmark] gpu_avg=" << s.avg_gpu_ms << "ms  gpu_p1=" << s.p1_gpu_ms
                  << "ms  gpu_frames=" << s.gpu_frame_count << '\n';
        std::cout << "[Benchmark] S1%_wall=" << s1_wall << "%  S1%_gpu=" << s1_gpu << "%\n";
    }
}

bool Benchmark::consume_finished() {
    if (just_finished_) {
        just_finished_ = false;
        return true;
    }
    return false;
}

bool Benchmark::tick(const float frame_dt, const renderer::FrameCpuTimings& cpu_timings) {
    if (!running_) {
        return false;
    }

    elapsed_s_ += frame_dt;

    if (elapsed_s_ > kWarmupDurationS) {
        frame_times_.push_back(frame_dt);

        if (collect_frame_trace_) {
            const bool fresh = cpu_timings.serial != renderer::kInvalidFrameSerial &&
                               cpu_timings.serial != last_trace_serial_;
            last_trace_serial_ = cpu_timings.serial;

            FrameSample sample{};
            sample.serial = fresh ? cpu_timings.serial : renderer::kInvalidFrameSerial;
            sample.wall_ms = frame_dt * 1000.0f;
            sample.gpu_ms = kNaN;
            if (!fresh) {
                // draw() did not submit this iteration (paused, or swapchain rebuild).
                sample.fence_wait_ms = kNaN;
                sample.acquire_ms = kNaN;
                sample.record_ms = kNaN;
                sample.submit_ms = kNaN;
                sample.present_ms = kNaN;
            } else {
                sample.fence_wait_ms = cpu_timings.fence_wait_ms;
                sample.acquire_ms = cpu_timings.acquire_ms;
                sample.record_ms = cpu_timings.record_ms;
                sample.submit_ms = cpu_timings.submit_ms;
                sample.present_ms = cpu_timings.present_ms;
                serial_to_row_.emplace(cpu_timings.serial, samples_.size());
            }
            samples_.push_back(sample);
        }

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

void Benchmark::apply_gpu_samples(const std::span<const renderer::GpuTimeSample> samples) {
    if (!running_ || elapsed_s_ <= kWarmupDurationS) {
        return;
    }

    for (const auto& sample : samples) {
        gpu_times_.push_back(sample.gpu_ms);

        if (!collect_frame_trace_) {
            continue;
        }
        // Samples resolve kMaxFramesInFlight frames after submission, so pair them by serial —
        // matching on arrival order would shift gpu_ms against wall_ms and destroy the per-frame
        // correlation this trace exists to show.
        const auto it = serial_to_row_.find(sample.serial);
        if (it != serial_to_row_.end()) {
            samples_[it->second].gpu_ms = sample.gpu_ms;
        }
    }
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

    result.gpu_frame_count = static_cast<int>(gpu_times_.size());
    if (!gpu_times_.empty()) {
        // Convert to seconds and reuse percentile_low_fps() verbatim, so S1%_gpu is defined
        // identically to the existing S1% and the two can be compared directly.
        std::vector<float> gpu_seconds;
        gpu_seconds.reserve(gpu_times_.size());
        for (const float ms : gpu_times_) {
            gpu_seconds.push_back(ms * 0.001f);
        }
        std::sort(gpu_seconds.begin(), gpu_seconds.end());

        float sum_gpu_fps = 0.0f;
        for (const float dt : gpu_seconds) {
            sum_gpu_fps += dt > 0.0f ? 1.0f / dt : 0.0f;
        }
        result.avg_gpu_fps = sum_gpu_fps / static_cast<float>(gpu_seconds.size());
        result.p1_low_gpu_fps = percentile_low_fps(gpu_seconds, 0.01f);

        auto gpu_ms_sorted = gpu_times_;
        std::sort(gpu_ms_sorted.begin(), gpu_ms_sorted.end());
        result.avg_gpu_ms = mean_of(gpu_ms_sorted);
        result.p1_gpu_ms = percentile_high_ms(gpu_ms_sorted, 0.01f);
    }

    if (!samples_.empty()) {
        stage_stats(samples_, &FrameSample::fence_wait_ms, result.avg_fence_wait_ms,
                    result.p1_high_fence_wait_ms);
        stage_stats(samples_, &FrameSample::acquire_ms, result.avg_acquire_ms,
                    result.p1_high_acquire_ms);
        stage_stats(samples_, &FrameSample::record_ms, result.avg_record_ms,
                    result.p1_high_record_ms);
        stage_stats(samples_, &FrameSample::submit_ms, result.avg_submit_ms,
                    result.p1_high_submit_ms);
        stage_stats(samples_, &FrameSample::present_ms, result.avg_present_ms,
                    result.p1_high_present_ms);
    }

    return result;
}

} // namespace engine
