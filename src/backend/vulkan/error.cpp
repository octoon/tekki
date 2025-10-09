#include "tekki/backend/vulkan/error.h"
#include <mutex>
#include <stdexcept>
#include <format>
#include <glm/glm.hpp>

namespace tekki::backend::vulkan {

uint32_t CrashMarkerNames::InsertName(const std::string& name) {
    // TODO: retire those with frames
    uint32_t idx = NextIdx;
    uint32_t smallIdx = idx % 4096;

    NextIdx = NextIdx + 1;
    Names[smallIdx] = std::make_pair(idx, name);

    return idx;
}

std::string CrashMarkerNames::GetName(uint32_t marker) const {
    auto it = Names.find(marker % 4096);
    if (it != Names.end()) {
        auto& [lastMarkerIdx, lastMarkerStr] = it->second;
        if (lastMarkerIdx == marker) {
            return lastMarkerStr;
        }
    }
    return "";
}

void Device::RecordCrashMarker(const std::shared_ptr<CommandBuffer>& cb, const std::string& name) {
    std::lock_guard<std::mutex> lock(CrashMarkerNamesMutex);
    uint32_t idx = CrashMarkerNamesObj.InsertName(name);

    try {
        Raw.cmd_fill_buffer(cb->GetRaw(), CrashTrackingBuffer->GetRaw(), 0, 4, idx);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::format("Failed to record crash marker: {}", e.what()));
    }
}

void Device::ReportError(const BackendError& err) {
    if (err.GetType() == BackendErrorType::Vulkan && 
        err.GetVulkanResult() == ash::vk::Result::ERROR_DEVICE_LOST) {
        
        // Something went very wrong. Find the last marker which was successfully written
        // to the crash tracking buffer, and report its corresponding name.
        uint32_t lastMarker = *reinterpret_cast<const uint32_t*>(
            CrashTrackingBuffer->GetAllocation().GetMappedPtr());

        std::lock_guard<std::mutex> lock(CrashMarkerNamesMutex);
        std::string lastMarkerName = CrashMarkerNamesObj.GetName(lastMarker);
        
        std::string msg;
        if (!lastMarkerName.empty()) {
            msg = std::format(
                "The GPU device has been lost. This is usually due to an infinite loop in a shader.\n"
                "The last crash marker was: {} => {}. The problem most likely exists directly after.",
                lastMarker, lastMarkerName
            );
        } else {
            msg = std::format(
                "The GPU device has been lost. This is usually due to an infinite loop in a shader.\n"
                "The last crash marker was: {}. The problem most likely exists directly after.", 
                lastMarker
            );
        }

        throw std::runtime_error(msg);
    }
    
    throw err;
}

} // namespace tekki::backend::vulkan