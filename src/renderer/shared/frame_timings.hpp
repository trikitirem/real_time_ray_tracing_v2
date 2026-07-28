#pragma once

#include <cstdint>
#include <limits>

namespace renderer {

// Monotonic per-frame id assigned by Renderer::draw(). GPU timestamp results resolve a couple of
// frames after the frame was submitted, so results are joined back to CPU-side measurements by
// this serial rather than by arrival order.
using FrameSerial = std::uint64_t;

inline constexpr FrameSerial kInvalidFrameSerial = std::numeric_limits<FrameSerial>::max();

// CPU-side cost of each blocking stage of a frame, known synchronously at the end of draw().
struct FrameCpuTimings {
    FrameSerial serial = kInvalidFrameSerial;
    float fence_wait_ms = 0.0f;
    float acquire_ms = 0.0f;
    float record_ms = 0.0f;
    float submit_ms = 0.0f;
    float present_ms = 0.0f;
};

// GPU execution span of one frame's command buffer, resolved once its fence has signalled.
struct GpuTimeSample {
    FrameSerial serial = kInvalidFrameSerial;
    float gpu_ms = 0.0f;
};

} // namespace renderer
