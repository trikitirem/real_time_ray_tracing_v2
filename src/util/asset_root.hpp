#pragma once

#include <filesystem>
#include <string_view>

namespace util {

[[nodiscard]] std::filesystem::path asset_root();
[[nodiscard]] std::filesystem::path resolve_asset(std::string_view relative);

} // namespace util
