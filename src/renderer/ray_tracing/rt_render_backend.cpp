#include "renderer/ray_tracing/rt_render_backend.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include "renderer/ray_tracing/rt_frame_recorder.hpp"
#include "renderer/ray_tracing/rt_gpu_types.hpp"
#include "renderer/ray_tracing/rt_pipeline.hpp"
#include "renderer/ray_tracing/shader_config.hpp"
#include "renderer/shared/buffers/buffer_kind.hpp"
#include "renderer/shared/descriptors/uniform_set.hpp"
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
    ctx_ = &ctx;
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

    camera_buffer_.emplace(
        ctx.physicalDevice(),
        ctx.device(),
        buffers::BufferKind::uniform,
        sizeof(CameraUbo));

    camera_uniform_set_.emplace(
        ctx.device(),
        descriptors::UniformSetConfig{
            .bindings = kCameraDescriptorBindings,
            .pool_sizes = kCameraDescriptorPoolSizes,
            .set_index = kCameraSetIndex,
            .max_sets = 1,
        });
    camera_uniform_set_->update_uniform_buffer(
        kCameraBinding,
        *camera_buffer_->buffer(),
        sizeof(CameraUbo));

    texture_uniform_sets_.clear();
    texture_uniform_sets_.reserve(kFramesInFlight);
    for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
        texture_uniform_sets_.emplace_back(
            ctx.device(),
            descriptors::UniformSetConfig{
                .bindings = kTextureDescriptorBindings,
                .pool_sizes = kTextureDescriptorPoolSizes,
                .set_index = kTextureSetIndex,
                .max_sets = 1,
            });
    }
    frame_recorder_->set_camera_uniform_set(
        camera_uniform_set_ ? &*camera_uniform_set_ : nullptr);
    if (!texture_uniform_sets_.empty()) {
        frame_recorder_->set_texture_uniform_set(&texture_uniform_sets_.front());
    }
}

void RtRenderBackend::destroy(DeviceContext& ctx)
{
    (void)ctx;
    tlas_build_ = {};
    blas_build_ = {};
    scene_data_ = {};
    reflection_instance_lut_buffer_.reset();
    texture_uniform_sets_.clear();
    camera_uniform_set_.reset();
    camera_buffer_.reset();
    frame_recorder_.reset();
    if (pipeline_) {
        pipeline_->destroy();
        pipeline_.reset();
    }
    destroy_depth_resources();
    ctx_ = nullptr;
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
    reflection_instance_lut_buffer_.reset();
    if (camera_uniform_set_) {
        if (scene_data_.material_buffer) {
            camera_uniform_set_->update_storage_buffer(
                kMaterialBufferBinding,
                *scene_data_.material_buffer->buffer(),
                scene_data_.material_buffer->size_bytes());
        }
        if (scene_data_.reflection_index_buffer) {
            camera_uniform_set_->update_storage_buffer(
                kReflectionIndexBufferBinding,
                *scene_data_.reflection_index_buffer->buffer(),
                scene_data_.reflection_index_buffer->size_bytes());
        }
        if (scene_data_.reflection_uv_buffer) {
            camera_uniform_set_->update_storage_buffer(
                kReflectionUvBufferBinding,
                *scene_data_.reflection_uv_buffer->buffer(),
                scene_data_.reflection_uv_buffer->size_bytes());
        }
        if (!scene_data_.reflection_instance_lut.empty() && ctx_ != nullptr) {
            std::vector<ReflectionInstanceLutGpu> reflection_lut_gpu{};
            reflection_lut_gpu.reserve(scene_data_.reflection_instance_lut.size());
            for (const ReflectionInstanceLutEntry& entry : scene_data_.reflection_instance_lut) {
                reflection_lut_gpu.push_back(ReflectionInstanceLutGpu{
                    .material_index = entry.material_index,
                    .index_offset = entry.index_offset,
                });
            }
            vk::CommandPoolCreateInfo pool_info{};
            pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
            pool_info.queueFamilyIndex = ctx_->graphicsQueueFamily();
            const vk::raii::CommandPool command_pool(ctx_->device(), pool_info);
            reflection_instance_lut_buffer_.emplace(buffers::GpuBuffer::from_span(
                ctx_->physicalDevice(),
                ctx_->device(),
                command_pool,
                ctx_->graphicsQueue(),
                buffers::BufferKind::storage,
                std::span<const ReflectionInstanceLutGpu>(
                    reflection_lut_gpu.data(),
                    reflection_lut_gpu.size())));
            camera_uniform_set_->update_storage_buffer(
                kReflectionInstanceLutBufferBinding,
                *reflection_instance_lut_buffer_->buffer(),
                reflection_instance_lut_buffer_->size_bytes());
        }
    }
    rebuild_acceleration_structures();
    if (frame_recorder_) {
        frame_recorder_->set_scene_data(&scene_data_);
    }
}

void RtRenderBackend::update_camera(const scene::Camera& camera, vk::Extent2D extent)
{
    if (!pipeline_) {
        return;
    }
    const float aspect = extent.height == 0 ? 1.0f
                                             : static_cast<float>(extent.width)
                                                   / static_cast<float>(extent.height);
    const CameraUbo ubo{
        .view_proj = camera.view_projection(aspect),
        .view_position = camera.position,
    };

    if (!camera_buffer_) {
        throw std::runtime_error("RtRenderBackend::update_camera requires initialized camera buffer");
    }
    camera_buffer_->update(std::span<const CameraUbo>(&ubo, 1));
}

void RtRenderBackend::record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx)
{
    FrameRecordContext rt_frame_ctx = frame_ctx;
    rt_frame_ctx.depthImage = *rt_depth_image_;
    frame_recorder_->set_camera_uniform_set(
        camera_uniform_set_ ? &*camera_uniform_set_ : nullptr);
    frame_recorder_->set_scene_data(&scene_data_);
    if (!texture_uniform_sets_.empty()) {
        const std::size_t idx = static_cast<std::size_t>(frame_ctx.frameIndex % texture_uniform_sets_.size());
        frame_recorder_->set_texture_uniform_set(&texture_uniform_sets_[idx]);
    } else {
        frame_recorder_->set_texture_uniform_set(nullptr);
    }
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

void RtRenderBackend::rebuild_acceleration_structures()
{
    tlas_build_ = {};
    blas_build_ = {};
    if (ctx_ == nullptr || !scene_data_.valid) {
        return;
    }
    blas_build_ = blas_builder_.build(*ctx_, scene_data_);
    tlas_build_ = tlas_builder_.build(*ctx_, blas_build_.instance_inputs);
    update_acceleration_structure_descriptor();
}

void RtRenderBackend::update_acceleration_structure_descriptor()
{
    if (ctx_ == nullptr || !camera_uniform_set_ || tlas_build_.tlas == nullptr) {
        return;
    }
    vk::WriteDescriptorSetAccelerationStructureKHR as_info{};
    as_info.accelerationStructureCount = 1;
    const vk::AccelerationStructureKHR tlas_handle = *tlas_build_.tlas;
    as_info.pAccelerationStructures = &tlas_handle;

    vk::WriteDescriptorSet as_write{};
    as_write.pNext = &as_info;
    as_write.dstSet = camera_uniform_set_->set();
    as_write.dstBinding = kAccelerationStructureBinding;
    as_write.dstArrayElement = 0;
    as_write.descriptorCount = 1;
    as_write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

    ctx_->device().updateDescriptorSets({ as_write }, {});
}

} // namespace renderer::ray_tracing
