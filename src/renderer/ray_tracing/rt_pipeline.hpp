#pragma once

#include <filesystem>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/shared/graphics_pipeline.hpp"

namespace renderer::ray_tracing {

class RayTracingPipeline : public renderer::GraphicsPipeline {
public:
    RayTracingPipeline() = default;
    RayTracingPipeline(const RayTracingPipeline&)            = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline&) = delete;
    RayTracingPipeline(RayTracingPipeline&&)                 = delete;
    RayTracingPipeline& operator=(RayTracingPipeline&&)      = delete;

    void create(const vk::raii::Device&        device,
                const vk::raii::PhysicalDevice& physicalDevice,
                vk::Format                     colorFormat,
                const std::filesystem::path&   spirv_path);
    void destroy();

private:
    vk::raii::DescriptorSetLayout camera_set_layout_ = nullptr;
};

} // namespace renderer::ray_tracing
