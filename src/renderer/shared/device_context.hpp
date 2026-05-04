#pragma once

#include <cstdint>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/shared/device_config.hpp"

struct GLFWwindow;

namespace renderer {

// Reads DeviceConfig via reference passed to init(); does not copy DeviceConfig.
// The referenced object must outlive DeviceContext (e.g. Engine member configs declared before deviceContext_).
class DeviceContext {
public:
    DeviceContext();
    DeviceContext(const DeviceContext&)            = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;
    DeviceContext(DeviceContext&&)                 = delete;
    DeviceContext& operator=(DeviceContext&&)      = delete;

    void init(GLFWwindow* window, const DeviceConfig& cfg);

    [[nodiscard]] const DeviceConfig& deviceConfig() const;

    [[nodiscard]] const vk::raii::Instance&       instance() const { return instance_; }
    [[nodiscard]] const vk::raii::SurfaceKHR&     surface() const { return surface_; }
    [[nodiscard]] const vk::raii::PhysicalDevice& physicalDevice() const { return physicalDevice_; }
    [[nodiscard]] const vk::raii::Device&         device() const { return device_; }
    [[nodiscard]] const vk::raii::Queue&          graphicsQueue() const { return graphicsQueue_; }
    [[nodiscard]] const vk::raii::Queue&          presentQueue() const { return presentQueue_; }

    [[nodiscard]] std::uint32_t graphicsQueueFamily() const { return graphicsQueueFamily_; }
    [[nodiscard]] std::uint32_t presentQueueFamily() const { return presentQueueFamily_; }

private:
    const DeviceConfig& cfg() const;

    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();

    vk::raii::Context                context_;
    vk::raii::Instance               instance_       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::SurfaceKHR             surface_        = nullptr;
    vk::raii::PhysicalDevice         physicalDevice_ = nullptr;
    vk::raii::Device                 device_         = nullptr;
    vk::raii::Queue                  graphicsQueue_  = nullptr;
    vk::raii::Queue                  presentQueue_   = nullptr;

    std::uint32_t graphicsQueueFamily_ = 0;
    std::uint32_t presentQueueFamily_  = 0;

    const DeviceConfig* cfgPtr_ = nullptr;
    bool                enableValidation_ = false;
};

} // namespace renderer
