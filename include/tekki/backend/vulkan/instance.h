#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <ash/ash.hpp>
#include "tekki/core/Result.h"

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
    
    ash::Entry GetEntry() const { return entry; }
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

    ash::Entry entry;
    VkInstance raw;
    VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
    std::unique_ptr<ash::extensions::ext::DebugReport> debugLoader;
    std::unique_ptr<ash::extensions::ext::DebugUtils> debugUtils;
    
    friend class DeviceBuilder;
};

} // namespace tekki::backend::vulkan