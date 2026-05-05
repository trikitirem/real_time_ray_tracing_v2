#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/shared/render_frame_recorder.hpp"
#include "renderer/shared/swapchain.hpp"

struct GLFWwindow;

namespace renderer {

class DeviceContext;

namespace raster {
class RasterPipeline;
}

namespace ray_tracing {
class RayTracingPipeline;
}

class Renderer {
public:
    Renderer(GLFWwindow* window, DeviceContext& ctx, bool useRasterBackend);
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    ~Renderer();

    void draw();

    void notifyFramebufferResized() { framebuffer_resized_ = true; }

private:
    void create_pipeline_resources();
    void destroy_pipeline_resources();

    void create_command_pool_and_buffers();
    void destroy_command_pool_and_buffers();

    void create_sync_objects();
    void destroy_sync_objects();

    void recreate_swapchain();

    void record_command_buffer(std::uint32_t frame_index, std::uint32_t image_index);

    void create_rt_depth_resources(vk::Extent2D extent);
    void destroy_rt_depth_resources();

    GLFWwindow*      window_;
    DeviceContext&   ctx_;
    const bool       use_raster_;

    Swapchain swapchain_;

    std::unique_ptr<raster::RasterPipeline>          raster_pipeline_;
    std::unique_ptr<ray_tracing::RayTracingPipeline> rt_pipeline_;

    vk::Format           rt_depth_format_ = vk::Format::eUndefined;
    vk::raii::Image      rt_depth_image_   = nullptr;
    vk::raii::DeviceMemory rt_depth_memory_ = nullptr;
    vk::raii::ImageView  rt_depth_view_    = nullptr;

    std::unique_ptr<RenderFrameRecorder> frame_recorder_;

    struct FrameSync {
        vk::raii::Semaphore image_available = nullptr;
        vk::raii::Fence     in_flight       = nullptr;
    };

    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    vk::raii::CommandPool              command_pool_     = nullptr;
    vk::raii::CommandBuffers           command_buffers_  = nullptr;
    std::vector<FrameSync>             frames_{};
    std::vector<vk::raii::Semaphore>   render_finished_{};

    std::uint32_t current_frame_        = 0;
    bool          framebuffer_resized_ = false;
};

} // namespace renderer
