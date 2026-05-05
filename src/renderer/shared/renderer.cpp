#include "renderer/shared/renderer.hpp"

#include "renderer/raster/raster_render_backend.hpp"
#include "renderer/raster/raster_scene_gpu_builder.hpp"
#include "renderer/ray_tracing/rt_render_backend.hpp"
#include "renderer/ray_tracing/rt_scene_gpu_builder.hpp"
#include "renderer/shared/device_context.hpp"
#include "scene/scene.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace renderer {

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

    if (use_raster_) {
        backend_ = std::make_unique<raster::RasterRenderBackend>();
    } else {
        backend_ = std::make_unique<ray_tracing::RtRenderBackend>();
    }
    backend_->create(ctx_, swapchain_);
    create_command_pool_and_buffers();
    create_sync_objects();
}

Renderer::~Renderer()
{
    ctx_.device().waitIdle();
    destroy_sync_objects();
    destroy_command_pool_and_buffers();
    backend_->destroy(ctx_);
}

void Renderer::load_scene(const scene::Scene& scene)
{
    loaded_scene_ = &scene;
    if (use_raster_) {
        raster::RasterSceneGpuBuilder builder{};
        backend_->load_scene(builder.build(ctx_, scene));
    } else {
        ray_tracing::RtSceneGpuBuilder builder{};
        backend_->load_scene(builder.build(ctx_, scene));
    }
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
    backend_->destroy(ctx_);

    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    swapchain_.recreate(ctx_, window_);

    backend_->create(ctx_, swapchain_);
    if (loaded_scene_ != nullptr) {
        load_scene(*loaded_scene_);
    }
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
    backend_->record(cmd, ctx);

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
