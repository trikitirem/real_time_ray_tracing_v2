#include "renderer/shared/shader_module_util.hpp"

#include <fstream>
#include <stdexcept>

namespace renderer {

std::vector<std::uint32_t> load_spirv_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("failed to open SPIR-V file: " + path.string());
    }

    const auto size = static_cast<std::size_t>(file.tellg());
    if (size == 0 || size % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("SPIR-V invalid size (must be non-zero multiple of 4): " + path.string());
    }

    std::vector<std::uint32_t> words(size / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size));
    return words;
}

vk::raii::ShaderModule make_shader_module(const vk::raii::Device& device,
                                          const std::vector<std::uint32_t>& code)
{
    vk::ShaderModuleCreateInfo ci{};
    ci.codeSize = code.size() * sizeof(std::uint32_t);
    ci.pCode    = code.data();
    return vk::raii::ShaderModule(device, ci);
}

} // namespace renderer
