#include "tekki/backend/vulkan/debug_utils.h"
#include <stdexcept>
#include <cstring>

namespace tekki::backend::vulkan {

DebugUtils::DebugUtils(VkDevice device, VkInstance instance) : device_(device) {
    // Load function pointers for VK_EXT_debug_utils extension
    vkSetDebugUtilsObjectNameEXT_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));

    vkSetDebugUtilsObjectTagEXT_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectTagEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectTagEXT"));

    vkCmdBeginDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));

    vkCmdEndDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));

    vkCmdInsertDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));

    vkQueueBeginDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT"));

    vkQueueEndDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT"));

    vkQueueInsertDebugUtilsLabelEXT_ = reinterpret_cast<PFN_vkQueueInsertDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkQueueInsertDebugUtilsLabelEXT"));
}

void DebugUtils::SetDebugUtilsObjectName(VkObjectType objectType, uint64_t objectHandle, const std::string& name) const {
    if (!vkSetDebugUtilsObjectNameEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.pNext = nullptr;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectNameEXT_(device_, &nameInfo);
}

void DebugUtils::SetDebugUtilsObjectTag(VkObjectType objectType, uint64_t objectHandle, uint64_t tagName, const void* tag, size_t tagSize) const {
    if (!vkSetDebugUtilsObjectTagEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsObjectTagInfoEXT tagInfo{};
    tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT;
    tagInfo.pNext = nullptr;
    tagInfo.objectType = objectType;
    tagInfo.objectHandle = objectHandle;
    tagInfo.tagName = tagName;
    tagInfo.tagSize = tagSize;
    tagInfo.pTag = tag;

    vkSetDebugUtilsObjectTagEXT_(device_, &tagInfo);
}

void DebugUtils::CmdBeginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const std::array<float, 4>& color) const {
    if (!vkCmdBeginDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = name.c_str();
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkCmdBeginDebugUtilsLabelEXT_(commandBuffer, &labelInfo);
}

void DebugUtils::CmdEndDebugUtilsLabel(VkCommandBuffer commandBuffer) const {
    if (!vkCmdEndDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    vkCmdEndDebugUtilsLabelEXT_(commandBuffer);
}

void DebugUtils::CmdInsertDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const std::array<float, 4>& color) const {
    if (!vkCmdInsertDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = name.c_str();
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkCmdInsertDebugUtilsLabelEXT_(commandBuffer, &labelInfo);
}

void DebugUtils::QueueBeginDebugUtilsLabel(VkQueue queue, const std::string& name, const std::array<float, 4>& color) const {
    if (!vkQueueBeginDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = name.c_str();
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkQueueBeginDebugUtilsLabelEXT_(queue, &labelInfo);
}

void DebugUtils::QueueEndDebugUtilsLabel(VkQueue queue) const {
    if (!vkQueueEndDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    vkQueueEndDebugUtilsLabelEXT_(queue);
}

void DebugUtils::QueueInsertDebugUtilsLabel(VkQueue queue, const std::string& name, const std::array<float, 4>& color) const {
    if (!vkQueueInsertDebugUtilsLabelEXT_) {
        return; // Extension not available
    }

    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = name.c_str();
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkQueueInsertDebugUtilsLabelEXT_(queue, &labelInfo);
}

} // namespace tekki::backend::vulkan
