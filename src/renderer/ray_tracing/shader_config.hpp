#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace renderer::ray_tracing {

// TEMP: only camera UBO; more bindings will be added in next steps.
inline constexpr std::uint32_t kCameraSetIndex = 0;
inline constexpr std::uint32_t kCameraBinding  = 0;

inline constexpr std::array<vk::DescriptorSetLayoutBinding, 1> kCameraDescriptorBindings = {
    vk::DescriptorSetLayoutBinding{
        .binding            = kCameraBinding,
        .descriptorType     = vk::DescriptorType::eUniformBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr,
    },
};

inline constexpr std::array<vk::DescriptorPoolSize, 1> kCameraDescriptorPoolSizes = {
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    },
};

} // namespace renderer::ray_tracing
