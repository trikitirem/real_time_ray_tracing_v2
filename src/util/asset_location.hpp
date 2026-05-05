#pragma once

#include <filesystem>
#include <string_view>

namespace util {

// Directory + file name
struct AssetLocation {
    std::filesystem::path directory;
    std::string_view      file_name;

    [[nodiscard]] std::filesystem::path full_path() const { return directory / file_name; }

    [[nodiscard]] std::filesystem::path resolve(std::string_view file) const
    {
        return directory / file;
    }
};

} // namespace util
