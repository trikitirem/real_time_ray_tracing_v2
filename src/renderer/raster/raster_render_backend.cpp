#include "renderer/raster/raster_render_backend.hpp"

#include "renderer/raster/raster_frame_recorder.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/raster/raster_gpu_types.hpp"
#include "renderer/raster/shader_config.hpp"
#include "renderer/shared/device_context.hpp"
#include "renderer/shared/swapchain.hpp"
#include "scene/camera.hpp"
#include "util/shader_paths.hpp"

#include <span>
#include <stdexcept>
#include <utility>

namespace renderer::raster {

RasterRenderBackend::~RasterRenderBackend() = default;

void RasterRenderBackend::create(DeviceContext& ctx, const Swapchain& swapchain)
{
    pipeline_ = std::make_unique<RasterPipeline>();
    pipeline_->create(ctx, swapchain, util::raster_shader("default.spv").full_path());
    frame_recorder_ = std::make_unique<RasterFrameRecorder>(*pipeline_);

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
    frame_recorder_->set_camera_uniform_set(&*camera_uniform_set_);
    if (!texture_uniform_sets_.empty()) {
        frame_recorder_->set_texture_uniform_set(&texture_uniform_sets_.front());
    }
}

void RasterRenderBackend::destroy(DeviceContext& ctx)
{
    (void)ctx;
    scene_data_ = {};
    texture_uniform_sets_.clear();
    camera_uniform_set_.reset();
    camera_buffer_.reset();
    frame_recorder_.reset();
    if (pipeline_) {
        pipeline_->destroy();
        pipeline_.reset();
    }
}

void RasterRenderBackend::load_scene(SceneGpuData&& scene_data)
{
    scene_data_ = std::move(scene_data);
    if (frame_recorder_) {
        frame_recorder_->set_scene_data(&scene_data_);
        if (!texture_uniform_sets_.empty()) {
            frame_recorder_->set_texture_uniform_set(&texture_uniform_sets_.front());
        } else {
            frame_recorder_->set_texture_uniform_set(nullptr);
        }
    }
}

void RasterRenderBackend::update_camera(const scene::Camera& camera, vk::Extent2D extent)
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
        throw std::runtime_error("RasterRenderBackend::update_camera requires initialized camera buffer");
    }
    camera_buffer_->update(std::span<const CameraUbo>(&ubo, 1));
}

void RasterRenderBackend::record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx)
{
    if (frame_recorder_) {
        frame_recorder_->set_scene_data(&scene_data_);
        frame_recorder_->set_camera_uniform_set(
            camera_uniform_set_ ? &*camera_uniform_set_ : nullptr);
        if (!texture_uniform_sets_.empty()) {
            const std::size_t idx = static_cast<std::size_t>(frame_ctx.frameIndex % texture_uniform_sets_.size());
            frame_recorder_->set_texture_uniform_set(&texture_uniform_sets_[idx]);
        } else {
            frame_recorder_->set_texture_uniform_set(nullptr);
        }
    }
    frame_recorder_->record(cmd, frame_ctx);
}

} // namespace renderer::raster
