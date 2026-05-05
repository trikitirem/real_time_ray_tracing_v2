#pragma once

#include "renderer/shared/render_frame_recorder.hpp"
#include "renderer/shared/scene_payload.hpp"

namespace engine {
class Camera;
}

namespace renderer {

class DeviceContext;
class Swapchain;

class IRenderBackend {
public:
    IRenderBackend() = default;
    IRenderBackend(const IRenderBackend&) = delete;
    IRenderBackend& operator=(const IRenderBackend&) = delete;
    virtual ~IRenderBackend() = default;

    virtual void create(DeviceContext& ctx, const Swapchain& swapchain) = 0;
    virtual void destroy(DeviceContext& ctx) = 0;
    virtual void load_scene(ScenePayload&& scene_payload) = 0;
    virtual void update_camera(const engine::Camera& camera, vk::Extent2D extent) = 0;
    virtual void record(vk::CommandBuffer cmd, const FrameRecordContext& frame_ctx) = 0;
};

} // namespace renderer
