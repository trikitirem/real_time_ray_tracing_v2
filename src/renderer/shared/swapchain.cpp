#include "renderer/shared/swapchain.hpp"

#include "renderer/shared/device_context.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace renderer {

namespace {

vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available)
{
    for (const auto& f : available) {
        if (f.format == vk::Format::eB8G8R8A8Srgb
            && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return f;
        }
    }
    if (!available.empty()) {
        return available.front();
    }
    return vk::SurfaceFormatKHR{
        .format     = vk::Format::eB8G8R8A8Srgb,
        .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
    };
}

vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& available)
{
    for (const auto mode : available) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps,
                              std::uint32_t                      fbWidth,
                              std::uint32_t                      fbHeight)
{
    if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return caps.currentExtent;
    }

    vk::Extent2D extent{ fbWidth, fbHeight };
    extent.width =
        std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

std::uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& caps)
{
    std::uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && count > caps.maxImageCount) {
        count = caps.maxImageCount;
    }
    return count;
}

} // namespace

void Swapchain::create(DeviceContext& ctx, int framebufferWidth, int framebufferHeight)
{
    createSwapchainResources(ctx, framebufferWidth, framebufferHeight);
}

void Swapchain::recreate(DeviceContext& ctx, GLFWwindow* window)
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    ctx.device().waitIdle();

    imageViews_.clear();
    swapchain_ = nullptr;
    images_.clear();

    createSwapchainResources(ctx, width, height);
}

void Swapchain::createSwapchainResources(DeviceContext& ctx, int framebufferWidth, int framebufferHeight)
{
    const auto&              physical  = ctx.physicalDevice();
    const vk::raii::Device&  device   = ctx.device();
    const vk::raii::SurfaceKHR& surface = ctx.surface();

    vk::SurfaceCapabilitiesKHR caps =
        physical.getSurfaceCapabilitiesKHR(*surface);

    const auto formats = physical.getSurfaceFormatsKHR(*surface);
    const auto presentModes = physical.getSurfacePresentModesKHR(*surface);

    surfaceFormat_ = chooseSurfaceFormat(formats);
    presentMode_   = choosePresentMode(presentModes);
    extent_ =
        chooseSwapExtent(caps,
                         static_cast<std::uint32_t>(std::max(0, framebufferWidth)),
                         static_cast<std::uint32_t>(std::max(0, framebufferHeight)));

    if (extent_.width == 0 || extent_.height == 0) {
        throw std::runtime_error("Swapchain extent is zero");
    }

    const std::uint32_t imageCount = chooseImageCount(caps);

    std::uint32_t qfiGraphics = ctx.graphicsQueueFamily();
    std::uint32_t qfiPresent  = ctx.presentQueueFamily();

    const vk::SharingMode sharingMode =
        (qfiGraphics != qfiPresent) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;

    std::array<std::uint32_t, 2> familyIndices = { qfiGraphics, qfiPresent };

    vk::SwapchainCreateInfoKHR ci{
        .surface               = *surface,
        .minImageCount         = imageCount,
        .imageFormat           = surfaceFormat_.format,
        .imageColorSpace       = surfaceFormat_.colorSpace,
        .imageExtent           = extent_,
        .imageArrayLayers      = 1,
        .imageUsage            = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode      = sharingMode,
        .queueFamilyIndexCount = (sharingMode == vk::SharingMode::eConcurrent) ? 2u : 0u,
        .pQueueFamilyIndices   = (sharingMode == vk::SharingMode::eConcurrent) ? familyIndices.data()
                                                                               : nullptr,
        .preTransform   = caps.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode    = presentMode_,
        .clipped        = true,
        .oldSwapchain   = {},
    };

    swapchain_ = vk::raii::SwapchainKHR(device, ci);
    images_    = swapchain_.getImages();

    imageViews_.clear();
    imageViews_.reserve(images_.size());

    for (const vk::Image& image : images_) {
        vk::ImageViewCreateInfo ivc{
            .image    = image,
            .viewType = vk::ImageViewType::e2D,
            .format   = surfaceFormat_.format,
            .subresourceRange =
                vk::ImageSubresourceRange{
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
        };
        imageViews_.emplace_back(device, ivc);
    }
}

} // namespace renderer
