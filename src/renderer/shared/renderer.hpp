#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "renderer/shared/frame_timings.hpp"
#include "renderer/shared/render_backend.hpp"
#include "renderer/shared/swapchain.hpp"

struct GLFWwindow;

namespace scene {
class Scene;
}

namespace engine {
class Camera;
}

namespace renderer {

class DeviceContext;

class Renderer {
  public:
    Renderer(GLFWwindow* window, DeviceContext& ctx, bool useRasterBackend);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    ~Renderer();

    void load_scene(const scene::Scene& scene);
    void set_camera(const engine::Camera& camera);
    void set_shadow_half_extent(float half_extent);
    void set_rt_reflections_enabled(bool enabled);
    void draw();
    void switch_backend(bool use_raster);

    [[nodiscard]] vk::Extent2D swapchain_extent() const {
        return swapchain_.extent();
    }
    [[nodiscard]] std::string present_mode_string() const;

    void notifyFramebufferResized() {
        framebuffer_resized_ = true;
    }

    [[nodiscard]] bool gpu_timing_enabled() const {
        return gpu_timing_enabled_;
    }

    // CPU stage costs of the most recent completed draw(). serial == kInvalidFrameSerial when the
    // last draw() bailed out before submitting (paused rendering, swapchain recreation).
    [[nodiscard]] const FrameCpuTimings& last_frame_cpu_timings() const {
        return last_cpu_timings_;
    }

    // Moves out every GPU sample resolved since the previous call. Samples arrive
    // kMaxFramesInFlight frames after submission, so callers must join them by
    // FrameCpuTimings::serial.
    void drain_gpu_samples(std::vector<GpuTimeSample>& out);

  private:
    void create_command_pool_and_buffers();
    void destroy_command_pool_and_buffers();

    void create_sync_objects();
    void destroy_sync_objects();

    void create_timestamp_pool();
    void destroy_timestamp_pool();
    void resolve_gpu_timestamps(std::uint32_t frame_index);

    void recreate_swapchain();

    void record_command_buffer(std::uint32_t frame_index, std::uint32_t image_index);

    GLFWwindow* window_;
    DeviceContext& ctx_;
    bool use_raster_;

    Swapchain swapchain_;
    std::unique_ptr<IRenderBackend> backend_;

    struct FrameSync {
        vk::raii::Semaphore image_available = nullptr;
        vk::raii::Fence in_flight = nullptr;
    };

    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    vk::raii::CommandPool command_pool_ = nullptr;
    vk::raii::CommandBuffers command_buffers_ = nullptr;
    std::vector<FrameSync> frames_{};
    std::vector<vk::raii::Semaphore> render_finished_{};
    const scene::Scene* loaded_scene_ = nullptr;
    const engine::Camera* camera_ = nullptr;

    std::uint32_t current_frame_ = 0;
    bool framebuffer_resized_ = false;

    // Whole-frame GPU timing. One pool, two timestamp slots per in-flight frame: 2*i and 2*i+1.
    vk::raii::QueryPool timestamp_pool_ = nullptr;
    bool gpu_timing_enabled_ = false;
    std::array<FrameSerial, kMaxFramesInFlight> slot_serial_{};
    std::array<bool, kMaxFramesInFlight> slot_pending_{};
    FrameSerial frame_serial_ = 0;
    std::vector<GpuTimeSample> resolved_gpu_{};
    FrameCpuTimings last_cpu_timings_{};
};

} // namespace renderer
