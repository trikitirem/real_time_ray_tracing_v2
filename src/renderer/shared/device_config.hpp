#pragma once

#include <vector>

namespace renderer {

struct DeviceConfig {
    std::vector<const char*> instanceLayers;
    std::vector<const char*> instanceExtensionsExtra;
    std::vector<const char*> deviceExtensions;
    void*                    deviceFeaturesChainHead = nullptr;
};

} // namespace renderer
