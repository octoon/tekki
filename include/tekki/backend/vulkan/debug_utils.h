#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <array>

namespace tekki::backend::vulkan {

// Debug utilities wrapper for VK_EXT_debug_utils extension
class DebugUtils {
public:
    // Constructor loads function pointers from the device
    DebugUtils(VkDevice device, VkInstance instance);
    ~DebugUtils() = default;

    DebugUtils(const DebugUtils&) = delete;
    DebugUtils& operator=(const DebugUtils&) = delete;
    DebugUtils(DebugUtils&&) = default;
    DebugUtils& operator=(DebugUtils&&) = default;

    // Set debug name for a Vulkan object
    void SetDebugUtilsObjectName(VkObjectType objectType, uint64_t objectHandle, const std::string& name) const;

    // Set debug tag for a Vulkan object
    void SetDebugUtilsObjectTag(VkObjectType objectType, uint64_t objectHandle, uint64_t tagName, const void* tag, size_t tagSize) const;

    // Command buffer debug labels
    void CmdBeginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const std::array<float, 4>& color = {1.0f, 1.0f, 1.0f, 1.0f}) const;
    void CmdEndDebugUtilsLabel(VkCommandBuffer commandBuffer) const;
    void CmdInsertDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name, const std::array<float, 4>& color = {1.0f, 1.0f, 1.0f, 1.0f}) const;

    // Queue debug labels
    void QueueBeginDebugUtilsLabel(VkQueue queue, const std::string& name, const std::array<float, 4>& color = {1.0f, 1.0f, 1.0f, 1.0f}) const;
    void QueueEndDebugUtilsLabel(VkQueue queue) const;
    void QueueInsertDebugUtilsLabel(VkQueue queue, const std::string& name, const std::array<float, 4>& color = {1.0f, 1.0f, 1.0f, 1.0f}) const;

private:
    VkDevice device_;

    // Function pointers for VK_EXT_debug_utils
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_ = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_ = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_ = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_ = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_ = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_ = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_ = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ = nullptr;
};

} // namespace tekki::backend::vulkan
