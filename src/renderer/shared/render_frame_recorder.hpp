#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace renderer {

struct FrameRecordContext {
    vk::Extent2D extent{};
    std::uint32_t imageIndex = 0;

    vk::Image swapchainImage{};
    vk::Image depthImage{};
};

class RenderFrameRecorder {
public:
    RenderFrameRecorder()                                  = default;
    RenderFrameRecorder(const RenderFrameRecorder&)            = delete;
    RenderFrameRecorder& operator=(const RenderFrameRecorder&) = delete;
    virtual ~RenderFrameRecorder()                           = default;

    virtual void record(vk::CommandBuffer cmd, const FrameRecordContext& ctx) = 0;
};

} // namespace renderer
