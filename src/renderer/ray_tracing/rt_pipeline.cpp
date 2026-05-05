#include "renderer/ray_tracing/rt_pipeline.hpp"

namespace renderer::ray_tracing {

void RayTracingPipeline::destroy()
{
    pipeline_        = nullptr;
    shader_module_   = nullptr;
    pipeline_layout_ = nullptr;
}

void RayTracingPipeline::create(const vk::raii::Device&        device,
                                const vk::raii::PhysicalDevice& physicalDevice,
                                vk::Format                     colorFormat,
                                const std::filesystem::path&   spirv_path)
{
    load_shader_module(device, spirv_path);
    create_empty_pipeline_layout(device);

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
