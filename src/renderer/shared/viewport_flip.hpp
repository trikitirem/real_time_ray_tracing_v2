#pragma once

#include <vulkan/vulkan.hpp>

namespace renderer {

// GLM-style clip space: flip Vulkan NDC Y using negative viewport height (VK_KHR_maintenance1).

[[nodiscard]] inline vk::Viewport make_viewport_y_up_ndc(vk::Extent2D extent)
{
    const float w = static_cast<float>(extent.width);
    const float h = static_cast<float>(extent.height);
    return vk::Viewport{ 0.0f, h, w, -h, 0.0f, 1.0f };
}

[[nodiscard]] inline vk::Rect2D make_full_framebuffer_scissor(vk::Extent2D extent)
{
    return vk::Rect2D{ vk::Offset2D{ 0, 0 }, extent };
}

} // namespace renderer
