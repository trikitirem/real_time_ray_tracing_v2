#include "renderer/raster/raster_frame_recorder.hpp"

#include <array>

#include "renderer/raster/raster_gpu_types.hpp"
#include "renderer/raster/raster_pipeline.hpp"
#include "renderer/shared/descriptors/uniform_set.hpp"
#include "renderer/shared/viewport_flip.hpp"

namespace renderer::raster {

RasterFrameRecorder::RasterFrameRecorder(RasterPipeline& pipeline)
    : pipeline_(pipeline)
{
}

void RasterFrameRecorder::record(vk::CommandBuffer cmd, const FrameRecordContext& ctx)
{
    vk::ClearValue clear_color{};
    clear_color.color
        = vk::ClearColorValue{ std::array<float, 4>{ 0.53f, 0.81f, 0.98f, 1.0f } };
    vk::ClearValue clear_depth{};
    clear_depth.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    const vk::ClearValue clears[] = { clear_color, clear_depth };

    vk::RenderPassBeginInfo begin{};
    begin.renderPass        = *pipeline_.render_pass();
    begin.framebuffer       = *pipeline_.framebuffers()[ctx.imageIndex];
    begin.renderArea.offset = vk::Offset2D{ 0, 0 };
    begin.renderArea.extent = ctx.extent;
    begin.clearValueCount   = static_cast<std::uint32_t>(std::size(clears));
    begin.pClearValues      = clears;

    cmd.beginRenderPass(begin, vk::SubpassContents::eInline);

    cmd.setViewport(0, make_viewport_y_up_ndc(ctx.extent));
    cmd.setScissor(0, make_full_framebuffer_scissor(ctx.extent));

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.pipeline());
    if (camera_uniform_set_) {
        camera_uniform_set_->bind(cmd, *pipeline_.pipeline_layout());
    }
    if (scene_data_ && scene_data_->valid) {
        for (const DrawItem& item : scene_data_->draw_items) {
            const glm::vec4 albedo
                = item.material_index < scene_data_->material_albedos.size()
                      ? scene_data_->material_albedos[item.material_index]
                      : glm::vec4(1.0f);
            const ModelPushConstant push{
                .model = item.model_matrix,
                .albedo = albedo,
            };
            cmd.pushConstants(*pipeline_.pipeline_layout(),
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                              0,
                              sizeof(ModelPushConstant),
                              &push);
            const vk::Buffer vb = item.vertex_buffer;
            const vk::DeviceSize vb_offset = 0;
            cmd.bindVertexBuffers(0, vb, vb_offset);
            cmd.bindIndexBuffer(item.index_buffer, 0, vk::IndexType::eUint32);
            cmd.drawIndexed(item.index_count, 1, item.first_index, item.vertex_offset, 0);
        }
    }

    cmd.endRenderPass();
}

} // namespace renderer::raster
