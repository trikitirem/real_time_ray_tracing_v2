#pragma once

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace renderer {
class DeviceContext;
}

namespace renderer::ray_tracing {

struct RtSceneGpuData;

struct BlasEntry {
    vk::raii::Buffer buffer = nullptr;
    vk::raii::DeviceMemory memory = nullptr;
    vk::raii::AccelerationStructureKHR handle = nullptr;
};

struct BlasInstanceInput {
    std::uint32_t geometry_index = 0;
    vk::DeviceAddress blas_address = 0;
    std::uint32_t material_index = 0;
    std::uint32_t index_offset = 0;
    glm::mat4 model_matrix = glm::mat4(1.0f);
};

struct BlasBuildResult {
    std::vector<BlasEntry> entries{};
    std::vector<BlasInstanceInput> instance_inputs{};
};

class BlasBuilder {
public:
    BlasBuildResult build(DeviceContext& ctx, const RtSceneGpuData& scene_data) const;
};

} // namespace renderer::ray_tracing
