#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace renderer::ray_tracing {

// TEMP: camera and one texture set; more bindings will be added in next steps.
inline constexpr std::uint32_t kCameraSetIndex = 0;
inline constexpr std::uint32_t kCameraBinding  = 0;
inline constexpr std::uint32_t kAccelerationStructureBinding = 1;
inline constexpr std::uint32_t kMaterialBufferBinding = 2;
inline constexpr std::uint32_t kReflectionIndexBufferBinding = 3;
inline constexpr std::uint32_t kReflectionUvBufferBinding = 4;
inline constexpr std::uint32_t kReflectionInstanceLutBufferBinding = 5;
inline constexpr std::uint32_t kTextureSetIndex = 1;
inline constexpr std::uint32_t kTextureImageBinding  = 0;
inline constexpr std::uint32_t kTextureSamplerBinding  = 1;

inline constexpr std::array<vk::DescriptorSetLayoutBinding, 6> kCameraDescriptorBindings = {
    vk::DescriptorSetLayoutBinding{
        .binding            = kCameraBinding,
        .descriptorType     = vk::DescriptorType::eUniformBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kAccelerationStructureBinding,
        .descriptorType     = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kMaterialBufferBinding,
        .descriptorType     = vk::DescriptorType::eStorageBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kReflectionIndexBufferBinding,
        .descriptorType     = vk::DescriptorType::eStorageBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kReflectionUvBufferBinding,
        .descriptorType     = vk::DescriptorType::eStorageBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kReflectionInstanceLutBufferBinding,
        .descriptorType     = vk::DescriptorType::eStorageBuffer,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
};

inline constexpr std::array<vk::DescriptorPoolSize, 3> kCameraDescriptorPoolSizes = {
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    },
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
    },
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 4,
    },
};

inline constexpr std::array<vk::DescriptorSetLayoutBinding, 2> kTextureDescriptorBindings = {
    vk::DescriptorSetLayoutBinding{
        .binding            = kTextureImageBinding,
        .descriptorType     = vk::DescriptorType::eSampledImage,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
    vk::DescriptorSetLayoutBinding{
        .binding            = kTextureSamplerBinding,
        .descriptorType     = vk::DescriptorType::eSampler,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr,
    },
};

inline constexpr std::array<vk::DescriptorPoolSize, 2> kTextureDescriptorPoolSizes = {
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eSampledImage,
        .descriptorCount = 1,
    },
    vk::DescriptorPoolSize{
        .type            = vk::DescriptorType::eSampler,
        .descriptorCount = 1,
    },
};

} // namespace renderer::ray_tracing
