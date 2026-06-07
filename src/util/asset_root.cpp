#include "util/asset_root.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <stdexcept>

namespace util {

namespace {

std::filesystem::path executable_directory()
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
#else
    char buffer[4096]{};
    const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return std::filesystem::current_path();
    }
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
#endif
}

} // namespace

std::filesystem::path asset_root()
{
    static const std::filesystem::path root = executable_directory();
    return root;
}

std::filesystem::path resolve_asset(const std::string_view relative)
{
    if (relative.empty()) {
        throw std::runtime_error("resolve_asset: empty relative path");
    }
    return asset_root() / std::filesystem::path(relative);
}

} // namespace util
