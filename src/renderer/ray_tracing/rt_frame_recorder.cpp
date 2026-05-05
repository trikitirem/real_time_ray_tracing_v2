#include "renderer/ray_tracing/rt_frame_recorder.hpp"

#include <array>

#include "renderer/ray_tracing/rt_pipeline.hpp"
#include "renderer/shared/swapchain.hpp"
#include "renderer/shared/viewport_flip.hpp"

namespace renderer::ray_tracing {

RayTracingFrameRecorder::RayTracingFrameRecorder(RayTracingPipeline&       pipeline,
                                                 const renderer::Swapchain& swapchain,
                                                 vk::Format               depthFormat,
                                                 vk::ImageView            depthImageView)
    : pipeline_(pipeline)
    , swapchain_(swapchain)
    , depth_format_(depthFormat)
    , depth_view_(depthImageView)
{
}

void RayTracingFrameRecorder::record(vk::CommandBuffer cmd, const FrameRecordContext& ctx)
{
    const vk::Image swap_img = ctx.swapchainImage;
    const vk::Image depth_img = ctx.depthImage;

    vk::ImageMemoryBarrier2 to_color{
        .srcStageMask     = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask    = {},
        .dstStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout        = vk::ImageLayout::eUndefined,
        .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
        .image            = swap_img,
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    const vk::ImageAspectFlags depth_aspect
        = (depth_format_ == vk::Format::eD32Sfloat)
        ? vk::ImageAspectFlagBits::eDepth
        : (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    vk::ImageMemoryBarrier2 to_depth{
        .srcStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests
                        | vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .dstStageMask  = vk::PipelineStageFlagBits2::eEarlyFragmentTests
                        | vk::PipelineStageFlagBits2::eLateFragmentTests,
        .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .oldLayout     = vk::ImageLayout::eUndefined,
        .newLayout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .image         = depth_img,
        .subresourceRange = {
            .aspectMask     = depth_aspect,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    const vk::ImageMemoryBarrier2 pre_barriers[] = { to_color, to_depth };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = static_cast<std::uint32_t>(std::size(pre_barriers)),
        .pImageMemoryBarriers  = pre_barriers,
    });

    vk::ClearValue clear_color{};
    clear_color.color
        = vk::ClearColorValue{ std::array<float, 4>{ 0.53f, 0.81f, 0.98f, 1.0f } };
    vk::ClearValue clear_depth{};
    clear_depth.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
    const vk::RenderingAttachmentInfo color_att{
        .imageView   = *swapchain_.imageViews()[ctx.imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = clear_color,
    };
    const vk::RenderingAttachmentInfo depth_att{
        .imageView   = depth_view_,
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eDontCare,
        .clearValue  = clear_depth,
    };

    const vk::RenderingInfo rendering{
        .renderArea           = { .offset = { 0, 0 }, .extent = ctx.extent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_att,
        .pDepthAttachment     = &depth_att,
    };

    cmd.beginRendering(rendering);

    cmd.setViewport(0, make_viewport_y_up_ndc(ctx.extent));
    cmd.setScissor(0, make_full_framebuffer_scissor(ctx.extent));

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.pipeline());
    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    vk::ImageMemoryBarrier2 to_present{
        .srcStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask     = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask    = {},
        .oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout        = vk::ImageLayout::ePresentSrcKHR,
        .image            = swap_img,
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers  = &to_present,
    });
}

} // namespace renderer::ray_tracing
