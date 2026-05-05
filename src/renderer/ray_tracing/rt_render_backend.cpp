#include "renderer/ray_tracing/rt_render_backend.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

#include "renderer/ray_tracing/rt_frame_recorder.hpp"
#include "renderer/ray_tracing/rt_pipeline.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/graphics_pipeline.hpp"
#include "renderer/shared/swapchain.hpp"
#include "scene/camera.hpp"
#include "util/shader_paths.hpp"

namespace renderer::ray_tracing {

namespace {

std::uint32_t find_memory_type(const vk::raii::PhysicalDevice& physicalDevice,
                               std::uint32_t typeFilter,
                               vk::MemoryPropertyFlags properties)
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

RtRenderBackend::~RtRenderBackend() = default;

void RtRenderBackend::create(DeviceContext& ctx, const Swapchain& swapchain)
{
    const vk::raii::Device& device = ctx.device();
    pipeline_ = std::make_unique<RayTracingPipeline>();
    pipeline_->create(device, ctx.physicalDevice(), swapchain.imageFormat(),
                      util::ray_tracing_shader("default.spv").full_path());
    create_depth_resources(ctx, swapchain.extent());
    frame_recorder_ = std::make_unique<RayTracingFrameRecorder>(
        *pipeline_,
        swapchain,
        rt_depth_format_,
        *rt_depth_view_);
}

void RtRenderBackend::destroy(DeviceContext& ctx)
{
    (void)ctx;
    scene_data_ = {};
    frame_recorder_.reset();
    if (pipeline_) {
        pipeline_->destroy();
        pipeline_.reset();
    }
    destroy_depth_resources();
}

void RtRenderBackend::load_scene(ScenePayload&& scene_payload)
{
    if (scene_payload.backend != BackendKind::ray_tracing) {
        throw std::runtime_error("RtRenderBackend::load_scene received non-ray-tracing payload");
    }
    RtSceneGpuData* typed = scene_payload.get_if<RtSceneGpuData>();
    if (typed == nullptr) {
        throw std::runtime_error("RtRenderBackend::load_scene payload type mismatch");
    }
    scene_data_ = std::move(*typed);
}

void RtRenderBackend::update_camera(const scene::Camera& camera, vk::Extent2D extent)
{
    (void)camera;
    (void)extent;
}

void RtRenderBackend::record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx)
{
    FrameRecordContext rt_frame_ctx = frame_ctx;
    rt_frame_ctx.depthImage = *rt_depth_image_;
    frame_recorder_->record(cmd, rt_frame_ctx);
}

void RtRenderBackend::create_depth_resources(DeviceContext& ctx, vk::Extent2D extent)
{
    const vk::raii::Device& device = ctx.device();
    const vk::raii::PhysicalDevice& physical = ctx.physicalDevice();

    rt_depth_format_ = GraphicsPipeline::find_depth_format(physical);

    vk::ImageCreateInfo depth_info{};
    depth_info.imageType = vk::ImageType::e2D;
    depth_info.extent = vk::Extent3D{ extent.width, extent.height, 1 };
    depth_info.mipLevels = 1;
    depth_info.arrayLayers = 1;
    depth_info.format = rt_depth_format_;
    depth_info.tiling = vk::ImageTiling::eOptimal;
    depth_info.initialLayout = vk::ImageLayout::eUndefined;
    depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depth_info.samples = vk::SampleCountFlagBits::e1;

    rt_depth_image_ = vk::raii::Image(device, depth_info);

    vk::ImageMemoryRequirementsInfo2 imreq{};
    imreq.image = *rt_depth_image_;
    const vk::MemoryRequirements2 mem_req2 = device.getImageMemoryRequirements2(imreq);
    const vk::MemoryRequirements& mem_req = mem_req2.memoryRequirements;

    const std::uint32_t mem_index = find_memory_type(
        physical, mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo alloc{};
    alloc.allocationSize = mem_req.size;
    alloc.memoryTypeIndex = mem_index;
    rt_depth_memory_ = vk::raii::DeviceMemory(device, alloc);

    vk::Device vk_dev(*device);
    vk_dev.bindImageMemory(*rt_depth_image_, *rt_depth_memory_, 0);

    vk::ImageViewCreateInfo view_info{};
    view_info.image = *rt_depth_image_;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = rt_depth_format_;
    view_info.subresourceRange.aspectMask
        = (rt_depth_format_ == vk::Format::eD32Sfloat)
              ? vk::ImageAspectFlagBits::eDepth
              : (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    rt_depth_view_ = vk::raii::ImageView(device, view_info);
}

void RtRenderBackend::destroy_depth_resources()
{
    rt_depth_view_ = nullptr;
    rt_depth_image_ = nullptr;
    rt_depth_memory_ = nullptr;
    rt_depth_format_ = vk::Format::eUndefined;
}

} // namespace renderer::ray_tracing
