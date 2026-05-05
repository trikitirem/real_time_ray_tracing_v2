#include "renderer/shared/renderer.hpp"

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/ray_tracing/rt_frame_recorder.hpp"
#include "renderer/ray_tracing/rt_pipeline.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/graphics_pipeline.hpp"

#include "util/shader_paths.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace renderer {

namespace {

std::uint32_t find_memory_type(const vk::raii::PhysicalDevice& physicalDevice,
                               std::uint32_t                 typeFilter,
                               vk::MemoryPropertyFlags        properties)
{
    const vk::PhysicalDeviceMemoryProperties mem = physicalDevice.getMemoryProperties();
    for (std::uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i))
            && (mem.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("find_memory_type: no suitable memory type");
}

} // namespace

Renderer::Renderer(GLFWwindow* window, DeviceContext& ctx, bool useRasterBackend)
    : window_(window)
    , ctx_(ctx)
    , use_raster_(useRasterBackend)
{
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w > 0 && h > 0) {
        swapchain_.create(ctx_, w, h);
    } else {
        swapchain_.create(ctx_, 1, 1);
    }

    create_pipeline_resources();
    create_command_pool_and_buffers();
    create_sync_objects();
}

Renderer::~Renderer()
{
    ctx_.device().waitIdle();
    destroy_sync_objects();
    destroy_command_pool_and_buffers();
    destroy_pipeline_resources();
}

void Renderer::create_rt_depth_resources(vk::Extent2D extent)
{
    const vk::raii::Device&         device   = ctx_.device();
    const vk::raii::PhysicalDevice& physical = ctx_.physicalDevice();

    rt_depth_format_ = GraphicsPipeline::find_depth_format(physical);

    const std::uint32_t w = extent.width;
    const std::uint32_t h = extent.height;

    vk::ImageCreateInfo depth_info{};
    depth_info.imageType     = vk::ImageType::e2D;
    depth_info.extent        = vk::Extent3D{ w, h, 1 };
    depth_info.mipLevels     = 1;
    depth_info.arrayLayers   = 1;
    depth_info.format        = rt_depth_format_;
    depth_info.tiling        = vk::ImageTiling::eOptimal;
    depth_info.initialLayout = vk::ImageLayout::eUndefined;
    depth_info.usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depth_info.samples       = vk::SampleCountFlagBits::e1;

    rt_depth_image_ = vk::raii::Image(device, depth_info);

    vk::ImageMemoryRequirementsInfo2 imreq{};
    imreq.image                            = *rt_depth_image_;
    const vk::MemoryRequirements2 mem_req2 = device.getImageMemoryRequirements2(imreq);
    const vk::MemoryRequirements& mem_req = mem_req2.memoryRequirements;

    const std::uint32_t mem_index
        = find_memory_type(physical, mem_req.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo alloc{};
    alloc.allocationSize  = mem_req.size;
    alloc.memoryTypeIndex = mem_index;
    rt_depth_memory_      = vk::raii::DeviceMemory(device, alloc);

    vk::Device vk_dev(*device);
    vk_dev.bindImageMemory(*rt_depth_image_, *rt_depth_memory_, 0);

    vk::ImageViewCreateInfo view_info{};
    view_info.image    = *rt_depth_image_;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format   = rt_depth_format_;
    view_info.subresourceRange.aspectMask
        = (rt_depth_format_ == vk::Format::eD32Sfloat)
        ? vk::ImageAspectFlagBits::eDepth
        : (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;

    rt_depth_view_ = vk::raii::ImageView(device, view_info);
}

void Renderer::destroy_rt_depth_resources()
{
    rt_depth_view_   = nullptr;
    rt_depth_image_  = nullptr;
    rt_depth_memory_ = nullptr;
    rt_depth_format_ = vk::Format::eUndefined;
}

void Renderer::create_pipeline_resources()
{
    if (use_raster_) {
        raster_pipeline_ = std::make_unique<raster::RasterPipeline>();
        raster_pipeline_->create(ctx_, swapchain_,
                                 util::raster_shader("default.spv").full_path());
        frame_recorder_
            = std::make_unique<raster::RasterFrameRecorder>(*raster_pipeline_);
    } else {
        const vk::raii::Device& device = ctx_.device();
        rt_pipeline_ = std::make_unique<ray_tracing::RayTracingPipeline>();
        rt_pipeline_->create(device, ctx_.physicalDevice(), swapchain_.imageFormat(),
                             util::ray_tracing_shader("default.spv").full_path());
        create_rt_depth_resources(swapchain_.extent());
        frame_recorder_ = std::make_unique<ray_tracing::RayTracingFrameRecorder>(
            *rt_pipeline_,
            swapchain_,
            rt_depth_format_,
            *rt_depth_view_);
    }
}

void Renderer::destroy_pipeline_resources()
{
    frame_recorder_.reset();

    if (raster_pipeline_) {
        raster_pipeline_->destroy();
        raster_pipeline_.reset();
    }
    if (rt_pipeline_) {
        rt_pipeline_->destroy();
        rt_pipeline_.reset();
    }
    destroy_rt_depth_resources();
}

void Renderer::create_command_pool_and_buffers()
{
    const vk::raii::Device& device = ctx_.device();

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = ctx_.graphicsQueueFamily();
    command_pool_              = vk::raii::CommandPool(device, pool_info);

    vk::CommandBufferAllocateInfo alloc{};
    alloc.commandPool        = *command_pool_;
    alloc.level              = vk::CommandBufferLevel::ePrimary;
    alloc.commandBufferCount = kMaxFramesInFlight;
    command_buffers_         = vk::raii::CommandBuffers(device, alloc);
}

void Renderer::destroy_command_pool_and_buffers()
{
    command_buffers_ = nullptr;
    command_pool_    = nullptr;
}

void Renderer::create_sync_objects()
{
    const vk::raii::Device& device = ctx_.device();

    const std::size_t image_count = swapchain_.images().size();
    render_finished_.clear();
    render_finished_.reserve(image_count);
    for (std::size_t i = 0; i < image_count; ++i) {
        render_finished_.emplace_back(device, vk::SemaphoreCreateInfo{});
    }

    frames_.clear();
    frames_.reserve(kMaxFramesInFlight);
    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        FrameSync f{};
        f.image_available = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo{});
        f.in_flight       = vk::raii::Fence(device,
                                      vk::FenceCreateInfo{
                                          .flags = vk::FenceCreateFlagBits::eSignaled,
                                      });
        frames_.push_back(std::move(f));
    }
}

void Renderer::destroy_sync_objects()
{
    render_finished_.clear();
    frames_.clear();
}

void Renderer::recreate_swapchain()
{
    ctx_.device().waitIdle();

    destroy_sync_objects();
    destroy_command_pool_and_buffers();
    destroy_pipeline_resources();

    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    swapchain_.recreate(ctx_, window_);

    create_pipeline_resources();
    create_command_pool_and_buffers();
    create_sync_objects();
}

void Renderer::record_command_buffer(const std::uint32_t frame_index,
                                     const std::uint32_t image_index)
{
    const vk::CommandBuffer cmd = *command_buffers_[frame_index];

    cmd.begin(vk::CommandBufferBeginInfo{});

    FrameRecordContext ctx{};
    ctx.extent         = swapchain_.extent();
    ctx.imageIndex     = image_index;
    ctx.swapchainImage = swapchain_.images()[image_index];
    if (!use_raster_) {
        ctx.depthImage = *rt_depth_image_;
    }

    frame_recorder_->record(cmd, ctx);

    cmd.end();
}

void Renderer::draw()
{
    if (framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    }

    const vk::raii::Device& device_raii = ctx_.device();
    const vk::Device        device      = *device_raii;

    if (frames_.empty() || render_finished_.empty()
        || command_buffers_.size() < kMaxFramesInFlight) {
        return;
    }

    FrameSync& frame = frames_[current_frame_];

    const vk::Result fence_wait
        = device.waitForFences({ *frame.in_flight }, vk::True, std::numeric_limits<std::uint64_t>::max());
    if (fence_wait != vk::Result::eSuccess) {
        throw std::runtime_error("Renderer::draw: waitForFences failed");
    }

    std::uint32_t image_index = 0;
    vk::Result    acq         = vk::Result::eSuccess;
    try {
        const auto r = device.acquireNextImageKHR(
            *swapchain_.handle(),
            std::numeric_limits<std::uint64_t>::max(),
            *frame.image_available,
            nullptr);
        acq         = r.result;
        image_index = r.value;
    } catch (const vk::SystemError& e) {
        if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
            recreate_swapchain();
            return;
        }
        throw;
    }

    if (acq == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }
    if (acq != vk::Result::eSuccess && acq != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Renderer::draw: acquireNextImageKHR failed");
    }

    device.resetFences({ *frame.in_flight });

    command_buffers_[current_frame_].reset({});

    record_command_buffer(current_frame_, image_index);

    const vk::PipelineStageFlags wait_stage
        = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    const vk::CommandBuffer cmd_buf = *command_buffers_[current_frame_];

    vk::SubmitInfo submit{};
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &*frame.image_available;
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd_buf;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &*render_finished_[image_index];

    ctx_.graphicsQueue().submit(submit, *frame.in_flight);

    const vk::SwapchainKHR swapchain_handle = *swapchain_.handle();
    vk::PresentInfoKHR     present{};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &*render_finished_[image_index];
    present.swapchainCount     = 1;
    present.pSwapchains        = &swapchain_handle;
    present.pImageIndices      = &image_index;

    const vk::Result present_result = ctx_.presentQueue().presentKHR(present);
    if (present_result == vk::Result::eErrorOutOfDateKHR
        || present_result == vk::Result::eSuboptimalKHR) {
        recreate_swapchain();
    } else if (present_result != vk::Result::eSuccess) {
        throw std::runtime_error("Renderer::draw: presentKHR failed");
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

} // namespace renderer
