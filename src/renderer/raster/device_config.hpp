#pragma once

#include <vulkan/vulkan.hpp>

#include "renderer/shared/device_config.hpp"

namespace renderer::raster {

namespace detail {

struct RasterFeatureChain {
    vk::PhysicalDeviceVulkan12Features       v12{};
    vk::PhysicalDeviceVulkan13Features       v13{};
    vk::PhysicalDeviceFeatures2              features2{};

    RasterFeatureChain()
    {
        v13.pNext            = &v12;
        v13.synchronization2 = vk::True;
        v13.dynamicRendering = vk::False;

        features2.pNext = &v13;
        features2.features.setSamplerAnisotropy(vk::True);
    }
};

} // namespace detail

inline DeviceConfig makeRasterDeviceConfig()
{
    static detail::RasterFeatureChain chain;

    DeviceConfig cfg{};
    cfg.deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
    };
    cfg.deviceFeaturesChainHead = &chain.features2;
    return cfg;
}

} // namespace renderer::raster
