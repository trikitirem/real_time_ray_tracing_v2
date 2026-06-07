#pragma once

#include "util/asset_location.hpp"

#include <filesystem>
#include <string>

// CMAKE passes SHADER_BUILD_ROOT (binary dir). Fallback for IDE parse without configure.
#ifndef SHADER_BUILD_ROOT
#    define SHADER_BUILD_ROOT "."
#endif

namespace util {

[[nodiscard]] inline std::filesystem::path shader_build_root()
{
    return std::filesystem::path(SHADER_BUILD_ROOT);
}

// Compiled SPIR-V from shaders/raster/*.slang -> <build>/shaders/raster/<name>.spv
[[nodiscard]] inline AssetLocation raster_shader(std::string_view spv_name)
{
    return AssetLocation{ shader_build_root() / "shaders" / "raster", std::string(spv_name) };
}

// Same for shaders/ray_tracing/
[[nodiscard]] inline AssetLocation ray_tracing_shader(std::string_view spv_name)
{
    return AssetLocation{ shader_build_root() / "shaders" / "ray_tracing", std::string(spv_name) };
}

} // namespace util
