#include "engine/frame_trace_writer.hpp"

#include "util/asset_root.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>

namespace engine {

namespace {

// Appends a float as a fixed-point field, or nothing at all when the value is missing. An empty
// CSV field is read back as NaN by pandas, which is what a skipped/unresolved measurement means.
void append_field(std::string& out, const float value) {
    out.push_back(',');
    if (std::isnan(value)) {
        return;
    }
    char buf[32];
    const int n = std::snprintf(buf, sizeof(buf), "%.5f", static_cast<double>(value));
    if (n > 0) {
        out.append(buf, static_cast<std::size_t>(n));
    }
}

} // namespace

std::filesystem::path write_frame_trace(const FrameTraceInfo& info,
                                        const std::vector<FrameSample>& samples) {
    if (samples.empty()) {
        return {};
    }

    const std::string filename =
        "frames_" + info.backend_key + "_" + std::to_string(info.stress_count) + "_run" +
        std::to_string(info.run_index + 1) + "_" + info.session_timestamp + ".csv";
    const std::filesystem::path path = util::resolve_asset("measurements") / filename;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[FrameTrace] Failed to write: " << path.string() << '\n';
        return {};
    }

    // Comment lines carry the run's identity; pandas skips them with comment="#".
    std::string buffer;
    buffer.reserve(samples.size() * 96 + 512);
    buffer += "# backend=" + info.backend_key +
              " stress_count=" + std::to_string(info.stress_count) +
              " run=" + std::to_string(info.run_index + 1) + '\n';
    buffer += "# gpu=" + info.gpu_name + " present_mode=" + info.present_mode + '\n';
    buffer += "frame,serial,wall_clock_time_ms,gpu_time_ms,cpu_record_ms,cpu_submit_ms,"
              "cpu_fence_wait_ms,cpu_acquire_ms,cpu_present_ms\n";

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const FrameSample& s = samples[i];
        buffer += std::to_string(i);
        buffer.push_back(',');
        if (s.serial != renderer::kInvalidFrameSerial) {
            buffer += std::to_string(s.serial);
        }
        append_field(buffer, s.wall_ms);
        append_field(buffer, s.gpu_ms);
        append_field(buffer, s.record_ms);
        append_field(buffer, s.submit_ms);
        append_field(buffer, s.fence_wait_ms);
        append_field(buffer, s.acquire_ms);
        append_field(buffer, s.present_ms);
        buffer.push_back('\n');
    }

    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!out) {
        std::cerr << "[FrameTrace] Write failed: " << path.string() << '\n';
        return {};
    }

    std::cout << "[FrameTrace] " << samples.size() << " frames -> " << filename << '\n';
    return path;
}

} // namespace engine
