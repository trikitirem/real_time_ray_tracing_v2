#pragma once

#include <vulkan/vulkan.hpp>

#include "renderer/shared/device_config.hpp"

namespace renderer::ray_tracing {

namespace detail {

struct RayTracingFeatureChain {
    vk::PhysicalDeviceRayQueryFeaturesKHR               rayQuery{};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR   asFeat{};
    vk::PhysicalDeviceVulkan11Features                v11{};
    vk::PhysicalDeviceVulkan12Features                v12{};
    vk::PhysicalDeviceVulkan13Features                v13{};
    vk::PhysicalDeviceFeatures2                       features2{};

    RayTracingFeatureChain()
    {
        rayQuery.rayQuery = vk::True;

        asFeat.pNext                 = &rayQuery;
        asFeat.accelerationStructure = vk::True;

        v12.pNext                           = &asFeat;
        v12.descriptorIndexing              = vk::True;
        v12.descriptorBindingPartiallyBound = vk::True;
        v12.runtimeDescriptorArray          = vk::True;
        v12.scalarBlockLayout               = vk::True;
        v12.bufferDeviceAddress             = vk::True;

        v11.pNext               = &v12;
        v11.shaderDrawParameters = vk::True;

        v13.pNext            = &v11;
        v13.synchronization2 = vk::True;
        v13.dynamicRendering = vk::True;

        features2.pNext = &v13;
        features2.features.setSamplerAnisotropy(vk::True);
    }
};

} // namespace detail

inline DeviceConfig makeRayTracingDeviceConfig()
{
    static detail::RayTracingFeatureChain chain;

    DeviceConfig cfg{};
    cfg.deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
    };
    cfg.deviceFeaturesChainHead = &chain.features2;
    return cfg;
}

} // namespace renderer::ray_tracing
