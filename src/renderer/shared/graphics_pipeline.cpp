#include "renderer/shared/graphics_pipeline.hpp"

#include "renderer/shared/shader_module_util.hpp"

#include <stdexcept>

namespace renderer {

vk::Format GraphicsPipeline::find_depth_format(const vk::raii::PhysicalDevice& pd)
{
    static constexpr vk::Format candidates[] = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };
    const auto required = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
    for (const vk::Format f : candidates) {
        const vk::FormatProperties props = pd.getFormatProperties(f);
        if ((props.optimalTilingFeatures & required) == required) {
            return f;
        }
    }
    throw std::runtime_error("no supported depth attachment format");
}

void GraphicsPipeline::load_shader_module(const vk::raii::Device&        device,
                                          const std::filesystem::path& spirv_path)
{
    shader_module_ = make_shader_module(device, load_spirv_file(spirv_path));
}

void GraphicsPipeline::create_empty_pipeline_layout(const vk::raii::Device& device)
{
    vk::PipelineLayoutCreateInfo layout_info{};
    pipeline_layout_ = vk::raii::PipelineLayout(device, layout_info);
}

void GraphicsPipeline::FixedFunctionState::init(vk::ShaderModule module)
{
    stages[0].stage  = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = module;
    stages[0].pName  = "vertMain";

    stages[1].stage  = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = module;
    stages[1].pName  = "fragMain";

    input_asm.topology = vk::PrimitiveTopology::eTriangleList;

    viewport.viewportCount = 1;
    viewport.scissorCount  = 1;

    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode    = vk::CullModeFlagBits::eBack;
    raster.frontFace   = vk::FrontFace::eCounterClockwise;
    raster.lineWidth   = 1.0f;

    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    ds.depthTestEnable  = vk::True;
    ds.depthWriteEnable = vk::True;
    ds.depthCompareOp   = vk::CompareOp::eLess;

    blend_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                               | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blend_att;

    dynamics[0] = vk::DynamicState::eViewport;
    dynamics[1] = vk::DynamicState::eScissor;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynamics;
}

void GraphicsPipeline::fill_graphics_pipeline_create_info(vk::GraphicsPipelineCreateInfo& gp,
                                                          FixedFunctionState&             state,
                                                          vk::PipelineLayout layout_handle,
                                                          vk::RenderPass      render_pass,
                                                          std::uint32_t       subpass)
{
    gp.stageCount          = 2;
    gp.pStages             = state.stages;
    gp.pVertexInputState   = &state.vertex_input;
    gp.pInputAssemblyState = &state.input_asm;
    gp.pViewportState      = &state.viewport;
    gp.pRasterizationState = &state.raster;
    gp.pMultisampleState   = &state.ms;
    gp.pDepthStencilState  = &state.ds;
    gp.pColorBlendState    = &state.blend;
    gp.pDynamicState       = &state.dyn;
    gp.layout              = layout_handle;
    gp.renderPass          = render_pass;
    gp.subpass             = subpass;
}

} // namespace renderer
