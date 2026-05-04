#pragma once

#include <vector>

#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;

namespace renderer {

class DeviceContext;

class Swapchain {
public:
    Swapchain() = default;
    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&)                 = delete;
    Swapchain& operator=(Swapchain&&)      = delete;

    void create(DeviceContext& ctx, int framebufferWidth, int framebufferHeight);
    void recreate(DeviceContext& ctx, GLFWwindow* window);

    [[nodiscard]] const vk::raii::SwapchainKHR& handle() const { return swapchain_; }
    [[nodiscard]] vk::Format                    imageFormat() const { return surfaceFormat_.format; }
    [[nodiscard]] const vk::SurfaceFormatKHR&   surfaceFormat() const { return surfaceFormat_; }
    [[nodiscard]] vk::Extent2D                 extent() const { return extent_; }
    [[nodiscard]] vk::PresentModeKHR            presentMode() const { return presentMode_; }

    [[nodiscard]] const std::vector<vk::Image>&            images() const { return images_; }
    [[nodiscard]] const std::vector<vk::raii::ImageView>& imageViews() const { return imageViews_; }

private:
    void createSwapchainResources(DeviceContext& ctx, int framebufferWidth, int framebufferHeight);

    vk::raii::SwapchainKHR           swapchain_      = nullptr;
    std::vector<vk::Image>           images_;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::SurfaceFormatKHR             surfaceFormat_{};
    vk::Extent2D                     extent_{};
    vk::PresentModeKHR               presentMode_{};
};

} // namespace renderer
