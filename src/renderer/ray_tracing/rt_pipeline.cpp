#include "renderer/ray_tracing/rt_pipeline.hpp"

#include "renderer/ray_tracing/shader_config.hpp"

#include <cstdint>
#include <iterator>

namespace renderer::ray_tracing {

void RayTracingPipeline::destroy()
{
    pipeline_        = nullptr;
    shader_module_   = nullptr;
    pipeline_layout_ = nullptr;
    camera_set_layout_ = nullptr;
    texture_set_layout_ = nullptr;
}

void RayTracingPipeline::create(const vk::raii::Device&        device,
                                const vk::raii::PhysicalDevice& physicalDevice,
                                vk::Format                     colorFormat,
                                const std::filesystem::path&   spirv_path)
{
    load_shader_module(device, spirv_path);
    vk::DescriptorSetLayoutCreateInfo camera_layout_ci{};
    camera_layout_ci.bindingCount = static_cast<std::uint32_t>(kCameraDescriptorBindings.size());
    camera_layout_ci.pBindings    = kCameraDescriptorBindings.data();
    camera_set_layout_            = vk::raii::DescriptorSetLayout(device, camera_layout_ci);

    vk::DescriptorSetLayoutCreateInfo texture_layout_ci{};
    texture_layout_ci.bindingCount = static_cast<std::uint32_t>(kTextureDescriptorBindings.size());
    texture_layout_ci.pBindings    = kTextureDescriptorBindings.data();
    texture_set_layout_            = vk::raii::DescriptorSetLayout(device, texture_layout_ci);

    const vk::DescriptorSetLayout set_layouts[] = { *camera_set_layout_, *texture_set_layout_ };
    vk::PipelineLayoutCreateInfo  pipeline_layout_ci{};
    pipeline_layout_ci.setLayoutCount = static_cast<std::uint32_t>(std::size(set_layouts));
    pipeline_layout_ci.pSetLayouts    = set_layouts;
    pipeline_layout_                  = vk::raii::PipelineLayout(device, pipeline_layout_ci);

    const vk::Format depth_format = find_depth_format(physicalDevice);

    FixedFunctionState ff{};
    ff.init(*shader_module_);

    vk::GraphicsPipelineCreateInfo gp{};
    fill_graphics_pipeline_create_info(gp, ff, *pipeline_layout_, nullptr, 0);

    vk::PipelineRenderingCreateInfo rendering{};
    rendering.colorAttachmentCount    = 1;
    rendering.pColorAttachmentFormats = &colorFormat;
    rendering.depthAttachmentFormat   = depth_format;
    rendering.stencilAttachmentFormat = vk::Format::eUndefined;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain(gp,
                                                                                               rendering);

    pipeline_
        = vk::raii::Pipeline(device, nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());
}

} // namespace renderer::ray_tracing
