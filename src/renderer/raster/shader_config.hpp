#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace renderer::raster {

// TEMP: camera and one texture set; more bindings will be added in next steps.
inline constexpr std::uint32_t kCameraSetIndex = 0;
inline constexpr std::uint32_t kCameraBinding  = 0;
inline constexpr std::uint32_t kTextureSetIndex = 1;
inline constexpr std::uint32_t kTextureBinding  = 0;

inline constexpr std::array<vk::DescriptorSetLayoutBinding, 1> kCameraDescriptorBindings = {
    vk::DescriptorSetLayoutBinding{
        .binding         = kCameraBinding,
        .descriptorType  = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags      = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr,
    },
};

inline constexpr std::array<vk::DescriptorPoolSize, 1> kCameraDescriptorPoolSizes = {
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    },
};

inline constexpr std::array<vk::DescriptorSetLayoutBinding, 1> kTextureDescriptorBindings = {
    vk::DescriptorSetLayoutBinding{
        .binding            = kTextureBinding,
        .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
};

inline constexpr std::array<vk::DescriptorPoolSize, 1> kTextureDescriptorPoolSizes = {
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
    },
};

} // namespace renderer::raster
