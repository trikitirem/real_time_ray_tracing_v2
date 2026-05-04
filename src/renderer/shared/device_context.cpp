#include "renderer/shared/device_context.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace renderer {

namespace {

constexpr std::array<const char*, 1> kValidationLayers = { "VK_LAYER_KHRONOS_validation" };

bool checkValidationLayerSupport(const vk::raii::Context& ctx)
{
    const auto available = ctx.enumerateInstanceLayerProperties();
    for (const char* layer : kValidationLayers) {
        bool found = false;
        for (const auto& a : available) {
            if (std::strcmp(a.layerName, layer) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
                                             VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[vk] " << data->pMessage << '\n';
    }
    return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo()
{
    vk::DebugUtilsMessengerCreateInfoEXT ci{};
    ci.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                       | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                       | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    ci.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                   | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                   | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    ci.pfnUserCallback =
        reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(&debugCallback);
    return ci;
}

std::vector<const char*> getGlfwRequiredInstanceExtensions()
{
    std::uint32_t           count    = 0;
    const char* const* glfwExt = glfwGetRequiredInstanceExtensions(&count);
    return { glfwExt, glfwExt + count };
}

bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& dev,
                                 const std::vector<const char*>& required)
{
    const auto            available = dev.enumerateDeviceExtensionProperties();
    std::set<std::string> missing(required.begin(), required.end());
    for (const auto& a : available) {
        missing.erase(a.extensionName);
    }
    return missing.empty();
}

} // namespace

DeviceContext::DeviceContext()
    : context_()
{
}

void DeviceContext::init(GLFWwindow* window, const DeviceConfig& cfg)
{
    config_ = cfg;

#ifndef NDEBUG
    enableValidation_ = true;
#endif
    if (enableValidation_ && !checkValidationLayerSupport(context_)) {
        std::cerr << "[vk] validation layers requested but not available; continuing without\n";
        enableValidation_ = false;
    }

    createInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
}

void DeviceContext::createInstance()
{
    vk::ApplicationInfo app{
        .pApplicationName   = "real_time_ray_tracing_v2",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName        = "rtrt_v2",
        .engineVersion      = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion         = VK_API_VERSION_1_3,
    };

    std::vector<const char*> layers = config_.instanceLayers;
    if (enableValidation_) {
        layers.insert(layers.end(), kValidationLayers.begin(), kValidationLayers.end());
    }

    std::vector<const char*> extensions = getGlfwRequiredInstanceExtensions();
    extensions.insert(extensions.end(),
                        config_.instanceExtensionsExtra.begin(),
                        config_.instanceExtensionsExtra.end());
    if (enableValidation_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::DebugUtilsMessengerCreateInfoEXT dbg = makeDebugMessengerCreateInfo();

    vk::InstanceCreateInfo instanceCreateInfo{
        .pNext                   = enableValidation_ ? &dbg : nullptr,
        .pApplicationInfo        = &app,
        .enabledLayerCount       = static_cast<std::uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    instance_ = vk::raii::Instance(context_, instanceCreateInfo);
}

void DeviceContext::setupDebugMessenger()
{
    if (!enableValidation_) {
        return;
    }
    debugMessenger_ = vk::raii::DebugUtilsMessengerEXT(instance_, makeDebugMessengerCreateInfo());
}

void DeviceContext::createSurface(GLFWwindow* window)
{
    VkSurfaceKHR raw = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance_), window, nullptr, &raw)
        != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_ = vk::raii::SurfaceKHR(instance_, raw);
}

void DeviceContext::pickPhysicalDevice()
{
    const auto devices = instance_.enumeratePhysicalDevices();

    for (const auto& dev : devices) {
        if (!checkDeviceExtensionSupport(dev, config_.deviceExtensions)) {
            continue;
        }

        const auto    qf       = dev.getQueueFamilyProperties();
        std::uint32_t graphics = UINT32_MAX;
        std::uint32_t present  = UINT32_MAX;
        for (std::uint32_t i = 0; i < qf.size(); ++i) {
            if (qf[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics = i;
            }
            if (dev.getSurfaceSupportKHR(i, *surface_)) {
                present = i;
            }
            if (graphics != UINT32_MAX && present != UINT32_MAX) {
                break;
            }
        }
        if (graphics == UINT32_MAX || present == UINT32_MAX) {
            continue;
        }

        physicalDevice_      = dev;
        graphicsQueueFamily_ = graphics;
        presentQueueFamily_  = present;
        return;
    }

    throw std::runtime_error(
        "No suitable Vulkan physical device found for current DeviceConfig (extensions / queues)");
}

void DeviceContext::createLogicalDevice()
{
    auto* features2 = static_cast<vk::PhysicalDeviceFeatures2*>(config_.deviceFeaturesChainHead);
    if (features2 == nullptr) {
        throw std::runtime_error("DeviceConfig::deviceFeaturesChainHead is null");
    }

    std::set<std::uint32_t> uniqueFamilies = { graphicsQueueFamily_, presentQueueFamily_ };
    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    const float                            prio = 1.0f;
    for (auto fam : uniqueFamilies) {
        queueInfos.push_back(vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = fam,
            .queueCount       = 1,
            .pQueuePriorities = &prio,
        });
    }

    vk::DeviceCreateInfo di{
        .pNext                   = features2,
        .queueCreateInfoCount    = static_cast<std::uint32_t>(queueInfos.size()),
        .pQueueCreateInfos       = queueInfos.data(),
        .enabledExtensionCount   = static_cast<std::uint32_t>(config_.deviceExtensions.size()),
        .ppEnabledExtensionNames = config_.deviceExtensions.data(),
    };

    device_        = vk::raii::Device(physicalDevice_, di);
    graphicsQueue_ = vk::raii::Queue(device_, graphicsQueueFamily_, 0);
    presentQueue_  = vk::raii::Queue(device_, presentQueueFamily_, 0);
}

} // namespace renderer
