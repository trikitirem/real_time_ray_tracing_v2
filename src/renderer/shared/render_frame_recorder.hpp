#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace renderer {

struct FrameRecordContext {
    vk::Extent2D extent{};
    std::uint32_t frameIndex = 0;
    std::uint32_t imageIndex = 0;

    vk::Image swapchainImage{};
    vk::Image depthImage{};
};

class FrameRecorder {
public:
    FrameRecorder()                                  = default;
    FrameRecorder(const FrameRecorder&)            = delete;
    FrameRecorder& operator=(const FrameRecorder&) = delete;
    virtual ~FrameRecorder()                           = default;

    virtual void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) = 0;
};

} // namespace renderer
