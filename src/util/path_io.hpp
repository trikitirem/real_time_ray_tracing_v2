#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

namespace util {

[[nodiscard]] inline std::string path_to_display_string(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

[[nodiscard]] inline FILE* open_binary_file(const std::filesystem::path& path)
{
#if defined(_WIN32)
    return _wfopen(path.wstring().c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

} // namespace util
