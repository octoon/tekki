#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"

// Vulkan extension function pointers
// In Rust, ash provides these, but in C++ we load them manually

namespace tekki::backend::vulkan {

class Instance;

class DeviceBuilder {
public:
    DeviceBuilder() = default;
    
    std::shared_ptr<Instance> Build();
    
    DeviceBuilder& RequiredExtensions(const std::vector<const char*>& requiredExtensions);
    DeviceBuilder& GraphicsDebugging(bool graphicsDebugging);

private:
    std::vector<const char*> requiredExtensions;
    bool graphicsDebugging = false;
    
    friend class Instance;
};

class Instance {
public:
    static DeviceBuilder Builder();

    VkInstance GetRaw() const { return raw; }

private:
    Instance() = default;

    static Instance Create(const DeviceBuilder& builder);

    static std::vector<const char*> ExtensionNames(const DeviceBuilder& builder);
    static std::vector<const char*> LayerNames(const DeviceBuilder& builder);

    static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObj,
        size_t location,
        int32_t msgCode,
        const char* layerPrefix,
        const char* message,
        void* userData);

    VkInstance raw = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;

    // Extension function pointers (replacing ash::extensions)
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = nullptr;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;

    friend class DeviceBuilder;
};

} // namespace tekki::backend::vulkan