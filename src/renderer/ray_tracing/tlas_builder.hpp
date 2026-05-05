#pragma once

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace renderer {
class DeviceContext;
}

namespace renderer::ray_tracing {

struct BlasInstanceInput;

struct TlasBuildResult {
    vk::raii::Buffer instance_buffer = nullptr;
    vk::raii::DeviceMemory instance_memory = nullptr;

    vk::raii::Buffer tlas_buffer = nullptr;
    vk::raii::DeviceMemory tlas_memory = nullptr;
    vk::raii::AccelerationStructureKHR tlas = nullptr;

    vk::raii::Buffer scratch_buffer = nullptr;
    vk::raii::DeviceMemory scratch_memory = nullptr;
};

class TlasBuilder {
public:
    TlasBuildResult build(DeviceContext& ctx, const std::vector<BlasInstanceInput>& instances) const;
};

} // namespace renderer::ray_tracing
