#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace renderer {

[[nodiscard]] std::vector<std::uint32_t> load_spirv_file(const std::filesystem::path& path);

[[nodiscard]] vk::raii::ShaderModule make_shader_module(const vk::raii::Device& device,
                                                         const std::vector<std::uint32_t>& code);

} // namespace renderer
