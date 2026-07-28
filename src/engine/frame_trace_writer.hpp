#pragma once

#include "engine/benchmark.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace engine {

// Per-frame CSV export for diagnostic runs. The regular stress suite does not use this: at ~137k
// frames per run it would produce gigabytes across the full 300-run session.
struct FrameTraceInfo {
    std::string backend_key;
    int stress_count = 0;
    int run_index = 0; // zero-based, written out as run_index + 1
    std::string session_timestamp;
    std::string gpu_name;
    std::string present_mode;
};

// Writes measurements/frames_<backend>_<count>_run<k>_<timestamp>.csv.
// Returns the path written, or an empty path on failure.
std::filesystem::path write_frame_trace(const FrameTraceInfo& info,
                                        const std::vector<FrameSample>& samples);

} // namespace engine
