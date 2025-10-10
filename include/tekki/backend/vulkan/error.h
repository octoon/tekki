#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <format>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"

namespace tekki::backend::vulkan {

enum class BackendErrorType {
    Allocation,
    Vulkan,
    ResourceAccess
};

class BackendError : public std::runtime_error {
public:
    BackendError(BackendErrorType type, const std::string& message)
        : std::runtime_error(message), type_(type), vkResult_(VK_SUCCESS) {}

    BackendError(VkResult vkResult, const std::string& message)
        : std::runtime_error(message), type_(BackendErrorType::Vulkan), vkResult_(vkResult) {}

    BackendErrorType GetType() const { return type_; }
    VkResult GetVulkanResult() const { return vkResult_; }

private:
    BackendErrorType type_;
    VkResult vkResult_;
};

class CrashMarkerNames {
public:
    CrashMarkerNames() : NextIdx(0) {}

    uint32_t InsertName(const std::string& name) {
        // TODO: retire those with frames
        uint32_t idx = NextIdx;
        uint32_t smallIdx = idx % 4096;

        NextIdx = NextIdx + 1;
        Names[smallIdx] = std::make_pair(idx, name);

        return idx;
    }

    std::string GetName(uint32_t marker) const {
        auto it = Names.find(marker % 4096);
        if (it != Names.end()) {
            auto& [lastMarkerIdx, lastMarkerStr] = it->second;
            if (lastMarkerIdx == marker) {
                return lastMarkerStr;
            }
        }
        return "";
    }

private:
    uint32_t NextIdx;
    std::unordered_map<uint32_t, std::pair<uint32_t, std::string>> Names;
};

// Error handling methods that should be added to the Device class
// These are declared here but implemented in the device.cpp file

} // namespace tekki::backend::vulkan