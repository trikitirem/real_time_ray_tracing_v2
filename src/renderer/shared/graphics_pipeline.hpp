#pragma once

#include <cstdint>
#include <filesystem>

#include <vulkan/vulkan_raii.hpp>

namespace renderer {

class GraphicsPipeline {
public:
    GraphicsPipeline()                     = default;
    GraphicsPipeline(const GraphicsPipeline&)            = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    virtual ~GraphicsPipeline() = default;

    [[nodiscard]] const vk::raii::PipelineLayout& pipeline_layout() const { return pipeline_layout_; }
    [[nodiscard]] const vk::raii::Pipeline&       pipeline() const { return pipeline_; }

    [[nodiscard]] static vk::Format find_depth_format(const vk::raii::PhysicalDevice& pd);

protected:

    void load_shader_module(const vk::raii::Device& device, const std::filesystem::path& spirv_path);
    void create_empty_pipeline_layout(const vk::raii::Device& device);

    struct FixedFunctionState {
        vk::PipelineShaderStageCreateInfo          stages[2]{};
        vk::PipelineVertexInputStateCreateInfo     vertex_input{};
        vk::PipelineInputAssemblyStateCreateInfo   input_asm{};
        vk::PipelineViewportStateCreateInfo        viewport{};
        vk::PipelineRasterizationStateCreateInfo   raster{};
        vk::PipelineMultisampleStateCreateInfo     ms{};
        vk::PipelineDepthStencilStateCreateInfo    ds{};
        vk::PipelineColorBlendAttachmentState    blend_att{};
        vk::PipelineColorBlendStateCreateInfo      blend{};
        vk::DynamicState                           dynamics[2]{};
        vk::PipelineDynamicStateCreateInfo         dyn{};

        void init(vk::ShaderModule module);
    };

    static void fill_graphics_pipeline_create_info(vk::GraphicsPipelineCreateInfo& gp,
                                                   FixedFunctionState&             state,
                                                   vk::PipelineLayout              layout_handle,
                                                   vk::RenderPass                   render_pass,
                                                   std::uint32_t                   subpass = 0);

    vk::raii::ShaderModule   shader_module_     = nullptr;
    vk::raii::PipelineLayout pipeline_layout_ = nullptr;
    vk::raii::Pipeline       pipeline_          = nullptr;
};

} // namespace renderer
