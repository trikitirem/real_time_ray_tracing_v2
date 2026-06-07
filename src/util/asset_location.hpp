#pragma once

#include <filesystem>
#include <string>

namespace util {

// Directory + file name (file_name stored by value — safe for paths built at runtime).
struct AssetLocation {
    std::filesystem::path directory;
    std::string           file_name;

    [[nodiscard]] std::filesystem::path full_path() const { return directory / file_name; }

    [[nodiscard]] std::filesystem::path resolve(const std::string_view file) const
    {
        return directory / file;
    }
};

} // namespace util
