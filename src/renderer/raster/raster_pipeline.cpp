#include "renderer/raster/raster_pipeline.hpp"

#include "renderer/raster/raster_gpu_types.hpp"
#include "renderer/raster/shader_config.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/swapchain.hpp"
#include "renderer/shared/shader_module_util.hpp"
#include "util/vertex.hpp"
#include "util/shader_paths.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace renderer::raster {

namespace {

std::uint32_t find_memory_type(const vk::raii::PhysicalDevice&  physicalDevice,
                               std::uint32_t                    typeFilter,
                               vk::MemoryPropertyFlags        properties)
{
    const vk::PhysicalDeviceMemoryProperties mem = physicalDevice.getMemoryProperties();
    for (std::uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i))
            && (mem.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type for depth image");
}

} // namespace

void RasterPipeline::destroy()
{
    framebuffers_.clear();
    pipeline_      = nullptr;
    render_pass_   = nullptr;
    depth_view_    = nullptr;
    depth_image_   = nullptr;
    depth_memory_  = nullptr;
    shader_module_   = nullptr;
    pipeline_layout_ = nullptr;
    camera_set_layout_ = nullptr;
    texture_set_layout_ = nullptr;
    shadow_framebuffer_ = nullptr;
    shadow_render_pass_ = nullptr;
    shadow_depth_view_ = nullptr;
    shadow_depth_image_ = nullptr;
    shadow_depth_memory_ = nullptr;
    shadow_sampler_ = nullptr;
    light_set_layout_ = nullptr;
    shadow_shader_module_ = nullptr;
    shadow_pipeline_layout_ = nullptr;
    shadow_pipeline_ = nullptr;
    depth_format_    = vk::Format::eUndefined;
}

void RasterPipeline::create(DeviceContext& ctx, const Swapchain& swapchain, const std::filesystem::path& spirv_path)
{
    const vk::raii::Device&         device   = ctx.device();
    const vk::raii::PhysicalDevice& physical = ctx.physicalDevice();

    depth_format_ = find_depth_format(physical);
    load_shader_module(device, spirv_path);

    const vk::Extent2D extent = swapchain.extent();

    create_depth_resources(device, physical, extent);
    create_render_pass(device, swapchain.imageFormat());
    vk::DescriptorSetLayoutCreateInfo camera_layout_ci{};
    camera_layout_ci.bindingCount = static_cast<std::uint32_t>(kCameraDescriptorBindings.size());
    camera_layout_ci.pBindings    = kCameraDescriptorBindings.data();
    camera_set_layout_            = vk::raii::DescriptorSetLayout(device, camera_layout_ci);

    vk::DescriptorSetLayoutCreateInfo texture_layout_ci{};
    texture_layout_ci.bindingCount = static_cast<std::uint32_t>(kTextureDescriptorBindings.size());
    texture_layout_ci.pBindings    = kTextureDescriptorBindings.data();
    texture_set_layout_            = vk::raii::DescriptorSetLayout(device, texture_layout_ci);

    vk::DescriptorSetLayoutCreateInfo light_layout_ci{};
    light_layout_ci.bindingCount = static_cast<std::uint32_t>(kLightDescriptorBindings.size());
    light_layout_ci.pBindings    = kLightDescriptorBindings.data();
    light_set_layout_            = vk::raii::DescriptorSetLayout(device, light_layout_ci);

    const vk::DescriptorSetLayout set_layouts[] = { *camera_set_layout_, *texture_set_layout_, *light_set_layout_ };
    vk::PushConstantRange model_push_range{};
    model_push_range.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    model_push_range.offset = 0;
    model_push_range.size = sizeof(ModelPushConstant);
    vk::PipelineLayoutCreateInfo  pipeline_layout_ci{};
    pipeline_layout_ci.setLayoutCount = static_cast<std::uint32_t>(std::size(set_layouts));
    pipeline_layout_ci.pSetLayouts    = set_layouts;
    pipeline_layout_ci.pushConstantRangeCount = 1;
    pipeline_layout_ci.pPushConstantRanges = &model_push_range;
    pipeline_layout_                  = vk::raii::PipelineLayout(device, pipeline_layout_ci);
    create_raster_graphics_pipeline(device);
    create_framebuffers(device, swapchain, extent);
    create_shadow_resources(device, physical);
    create_shadow_render_pass(device);
    create_shadow_framebuffer(device);
    create_shadow_pipeline(device, util::raster_shader("shadow.spv").full_path());
}

void RasterPipeline::create_depth_resources(const vk::raii::Device&         device,
                                            const vk::raii::PhysicalDevice& physical,
                                            vk::Extent2D                   extent)
{
    const std::uint32_t w = extent.width;
    const std::uint32_t h = extent.height;

    vk::ImageCreateInfo depth_image_info{};
    depth_image_info.imageType     = vk::ImageType::e2D;
    depth_image_info.extent        = vk::Extent3D{ w, h, 1 };
    depth_image_info.mipLevels     = 1;
    depth_image_info.arrayLayers   = 1;
    depth_image_info.format        = depth_format_;
    depth_image_info.tiling        = vk::ImageTiling::eOptimal;
    depth_image_info.initialLayout = vk::ImageLayout::eUndefined;
    depth_image_info.usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depth_image_info.samples       = vk::SampleCountFlagBits::e1;
    depth_image_                   = vk::raii::Image(device, depth_image_info);

    vk::ImageMemoryRequirementsInfo2 imreq{};
    imreq.image                            = *depth_image_;
    const vk::MemoryRequirements2 mem_req2 = device.getImageMemoryRequirements2(imreq);
    const vk::MemoryRequirements& mem_req = mem_req2.memoryRequirements;

    const std::uint32_t mem_type = find_memory_type(physical, mem_req.memoryTypeBits,
                                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize  = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type;
    depth_memory_              = vk::raii::DeviceMemory(device, alloc_info);

    vk::Device vk_device(*device);
    vk_device.bindImageMemory(*depth_image_, *depth_memory_, 0);

    vk::ImageViewCreateInfo depth_view_info{};
    depth_view_info.image    = *depth_image_;
    depth_view_info.viewType = vk::ImageViewType::e2D;
    depth_view_info.format   = depth_format_;
    depth_view_info.subresourceRange.aspectMask
        = (depth_format_ == vk::Format::eD32Sfloat) ? vk::ImageAspectFlagBits::eDepth
              : (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    depth_view_info.subresourceRange.levelCount = 1;
    depth_view_info.subresourceRange.layerCount = 1;
    depth_view_                                 = vk::raii::ImageView(device, depth_view_info);
}

void RasterPipeline::create_render_pass(const vk::raii::Device& device, vk::Format swapchain_color_format)
{
    vk::AttachmentDescription color_att{};
    color_att.format         = swapchain_color_format;
    color_att.samples        = vk::SampleCountFlagBits::e1;
    color_att.loadOp         = vk::AttachmentLoadOp::eClear;
    color_att.storeOp        = vk::AttachmentStoreOp::eStore;
    color_att.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    color_att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_att.initialLayout  = vk::ImageLayout::eUndefined;
    color_att.finalLayout    = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentDescription depth_att{};
    depth_att.format         = depth_format_;
    depth_att.samples        = vk::SampleCountFlagBits::e1;
    depth_att.loadOp         = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp        = vk::AttachmentStoreOp::eDontCare;
    depth_att.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    depth_att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_att.initialLayout  = vk::ImageLayout::eUndefined;
    depth_att.finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    const vk::AttachmentReference color_ref{ 0, vk::ImageLayout::eColorAttachmentOptimal };
    const vk::AttachmentReference depth_ref{ 1, vk::ImageLayout::eDepthStencilAttachmentOptimal };

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint       = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    vk::SubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask  = vk::PipelineStageFlagBits::eTopOfPipe;
    dep.dstStageMask  = vk::PipelineStageFlagBits::eEarlyFragmentTests
                       | vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dep.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite
                       | vk::AccessFlagBits::eColorAttachmentWrite;

    const vk::AttachmentDescription attachments[] = { color_att, depth_att };

    vk::RenderPassCreateInfo rp_info{};
    rp_info.attachmentCount = static_cast<std::uint32_t>(std::size(attachments));
    rp_info.pAttachments    = attachments;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dep;

    render_pass_ = vk::raii::RenderPass(device, rp_info);
}

void RasterPipeline::create_raster_graphics_pipeline(const vk::raii::Device& device)
{
    FixedFunctionState ff{};
    ff.init(*shader_module_);

    static const vk::VertexInputBindingDescription binding{
        .binding = 0,
        .stride = sizeof(util::Vertex),
        .inputRate = vk::VertexInputRate::eVertex,
    };
    static const vk::VertexInputAttributeDescription attributes[] = {
        vk::VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<std::uint32_t>(offsetof(util::Vertex, position)),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<std::uint32_t>(offsetof(util::Vertex, uv)),
        },
        vk::VertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<std::uint32_t>(offsetof(util::Vertex, normal)),
        },
    };
    ff.vertex_input.vertexBindingDescriptionCount = 1;
    ff.vertex_input.pVertexBindingDescriptions = &binding;
    ff.vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(attributes));
    ff.vertex_input.pVertexAttributeDescriptions = attributes;

    vk::GraphicsPipelineCreateInfo gp{};
    fill_graphics_pipeline_create_info(gp, ff, *pipeline_layout_, *render_pass_, 0);
    pipeline_ = vk::raii::Pipeline(device, nullptr, gp);
}

void RasterPipeline::create_framebuffers(const vk::raii::Device& device,
                                         const Swapchain&        swapchain,
                                         vk::Extent2D            extent)
{
    const std::uint32_t w = extent.width;
    const std::uint32_t h = extent.height;

    const auto& views = swapchain.imageViews();
    framebuffers_.clear();
    framebuffers_.reserve(views.size());

    for (const auto& color_view : views) {
        const vk::ImageView fb_attachments[] = { *color_view, *depth_view_ };
        vk::FramebufferCreateInfo fb_info{};
        fb_info.renderPass      = *render_pass_;
        fb_info.attachmentCount = static_cast<std::uint32_t>(std::size(fb_attachments));
        fb_info.pAttachments    = fb_attachments;
        fb_info.width           = w;
        fb_info.height          = h;
        fb_info.layers          = 1;
        framebuffers_.emplace_back(device, fb_info);
    }
}

void RasterPipeline::create_shadow_resources(const vk::raii::Device& device,
                                             const vk::raii::PhysicalDevice& physical)
{
    constexpr std::uint32_t sz = kShadowMapSize;

    vk::ImageCreateInfo img_info{};
    img_info.imageType = vk::ImageType::e2D;
    img_info.extent = vk::Extent3D{ sz, sz, 1 };
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.format = depth_format_;
    img_info.tiling = vk::ImageTiling::eOptimal;
    img_info.initialLayout = vk::ImageLayout::eUndefined;
    img_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment
                   | vk::ImageUsageFlagBits::eSampled;
    img_info.samples = vk::SampleCountFlagBits::e1;
    shadow_depth_image_ = vk::raii::Image(device, img_info);

    vk::ImageMemoryRequirementsInfo2 imreq{};
    imreq.image = *shadow_depth_image_;
    const vk::MemoryRequirements2 mem_req2 = device.getImageMemoryRequirements2(imreq);
    const vk::MemoryRequirements& mem_req = mem_req2.memoryRequirements;

    const std::uint32_t mem_type = find_memory_type(
        physical, mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type;
    shadow_depth_memory_ = vk::raii::DeviceMemory(device, alloc_info);

    vk::Device vk_dev(*device);
    vk_dev.bindImageMemory(*shadow_depth_image_, *shadow_depth_memory_, 0);

    vk::ImageViewCreateInfo view_info{};
    view_info.image = *shadow_depth_image_;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = depth_format_;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    shadow_depth_view_ = vk::raii::ImageView(device, view_info);

    vk::SamplerCreateInfo samp_info{};
    samp_info.magFilter = vk::Filter::eLinear;
    samp_info.minFilter = vk::Filter::eLinear;
    samp_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
    samp_info.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    samp_info.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    samp_info.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    samp_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    samp_info.compareEnable = vk::True;
    samp_info.compareOp = vk::CompareOp::eLessOrEqual;
    shadow_sampler_ = vk::raii::Sampler(device, samp_info);
}

void RasterPipeline::create_shadow_render_pass(const vk::raii::Device& device)
{
    vk::AttachmentDescription depth_att{};
    depth_att.format = depth_format_;
    depth_att.samples = vk::SampleCountFlagBits::e1;
    depth_att.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depth_att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_att.initialLayout = vk::ImageLayout::eUndefined;
    depth_att.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    const vk::AttachmentReference depth_ref{ 0, vk::ImageLayout::eDepthStencilAttachmentOptimal };

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depth_ref;

    std::array<vk::SubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    deps[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
    deps[0].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
    deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    deps[1].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;

    vk::RenderPassCreateInfo rp_info{};
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &depth_att;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = static_cast<std::uint32_t>(deps.size());
    rp_info.pDependencies = deps.data();

    shadow_render_pass_ = vk::raii::RenderPass(device, rp_info);
}

void RasterPipeline::create_shadow_framebuffer(const vk::raii::Device& device)
{
    const vk::ImageView attachment = *shadow_depth_view_;

    vk::FramebufferCreateInfo fb_info{};
    fb_info.renderPass = *shadow_render_pass_;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &attachment;
    fb_info.width = kShadowMapSize;
    fb_info.height = kShadowMapSize;
    fb_info.layers = 1;
    shadow_framebuffer_ = vk::raii::Framebuffer(device, fb_info);
}

void RasterPipeline::create_shadow_pipeline(const vk::raii::Device& device,
                                            const std::filesystem::path& shadow_spirv_path)
{
    shadow_shader_module_ = make_shader_module(device, load_spirv_file(shadow_spirv_path));

    FixedFunctionState ff{};
    ff.stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    ff.stages[0].module = *shadow_shader_module_;
    ff.stages[0].pName = "vertMain";

    static const vk::VertexInputBindingDescription binding{
        .binding = 0,
        .stride = sizeof(util::Vertex),
        .inputRate = vk::VertexInputRate::eVertex,
    };
    static const vk::VertexInputAttributeDescription attr{
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = static_cast<std::uint32_t>(offsetof(util::Vertex, position)),
    };
    ff.vertex_input.vertexBindingDescriptionCount = 1;
    ff.vertex_input.pVertexBindingDescriptions = &binding;
    ff.vertex_input.vertexAttributeDescriptionCount = 1;
    ff.vertex_input.pVertexAttributeDescriptions = &attr;

    ff.input_asm.topology = vk::PrimitiveTopology::eTriangleList;
    ff.viewport.viewportCount = 1;
    ff.viewport.scissorCount = 1;
    ff.raster.polygonMode = vk::PolygonMode::eFill;
    ff.raster.cullMode = vk::CullModeFlagBits::eNone;
    ff.raster.frontFace = vk::FrontFace::eCounterClockwise;
    ff.raster.lineWidth = 1.0f;
    ff.raster.depthBiasEnable = vk::True;
    ff.raster.depthBiasConstantFactor = 1.25f;
    ff.raster.depthBiasSlopeFactor = 1.75f;
    ff.ms.rasterizationSamples = vk::SampleCountFlagBits::e1;
    ff.ds.depthTestEnable = vk::True;
    ff.ds.depthWriteEnable = vk::True;
    ff.ds.depthCompareOp = vk::CompareOp::eLessOrEqual;
    ff.blend.attachmentCount = 0;

    ff.dynamics[0] = vk::DynamicState::eViewport;
    ff.dynamics[1] = vk::DynamicState::eScissor;
    ff.dyn.dynamicStateCount = 2;
    ff.dyn.pDynamicStates = ff.dynamics;

    vk::PushConstantRange shadow_push{};
    shadow_push.stageFlags = vk::ShaderStageFlagBits::eVertex;
    shadow_push.offset = 0;
    shadow_push.size = sizeof(ShadowPushConstant);

    vk::PipelineLayoutCreateInfo shadow_layout_ci{};
    shadow_layout_ci.pushConstantRangeCount = 1;
    shadow_layout_ci.pPushConstantRanges = &shadow_push;
    shadow_pipeline_layout_ = vk::raii::PipelineLayout(device, shadow_layout_ci);

    vk::GraphicsPipelineCreateInfo gp{};
    gp.stageCount = 1;
    gp.pStages = ff.stages;
    gp.pVertexInputState = &ff.vertex_input;
    gp.pInputAssemblyState = &ff.input_asm;
    gp.pViewportState = &ff.viewport;
    gp.pRasterizationState = &ff.raster;
    gp.pMultisampleState = &ff.ms;
    gp.pDepthStencilState = &ff.ds;
    gp.pColorBlendState = &ff.blend;
    gp.pDynamicState = &ff.dyn;
    gp.layout = *shadow_pipeline_layout_;
    gp.renderPass = *shadow_render_pass_;
    gp.subpass = 0;

    shadow_pipeline_ = vk::raii::Pipeline(device, nullptr, gp);
}

} // namespace renderer::raster
