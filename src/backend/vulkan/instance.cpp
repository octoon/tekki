#include "tekki/backend/vulkan/instance.h"
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include <stdexcept>

namespace tekki::backend::vulkan {

std::shared_ptr<Instance> DeviceBuilder::Build() {
    try {
        return std::make_shared<Instance>(Instance::Create(*this));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to build instance: ") + e.what());
    }
}

DeviceBuilder& DeviceBuilder::RequiredExtensions(const std::vector<const char*>& exts) {
    this->requiredExtensions = exts;
    return *this;
}

DeviceBuilder& DeviceBuilder::GraphicsDebugging(bool debug) {
    this->graphicsDebugging = debug;
    return *this;
}

DeviceBuilder Instance::Builder() {
    return DeviceBuilder();
}

Instance Instance::Create(const DeviceBuilder& builder) {
    Instance instance;

    // Get extension names
    auto extensionNames = Instance::ExtensionNames(builder);

    // Get layer names
    auto layerNames = Instance::LayerNames(builder);

    // Combine required extensions with additional extensions
    std::vector<const char*> allExtensions = builder.requiredExtensions;
    allExtensions.insert(allExtensions.end(), extensionNames.begin(), extensionNames.end());

    // Create application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);

    // Create instance info
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(allExtensions.size());
    instanceInfo.ppEnabledExtensionNames = allExtensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
    instanceInfo.ppEnabledLayerNames = layerNames.data();

    // Create Vulkan instance
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance.raw);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    // Setup debug reporting if enabled
    if (builder.graphicsDebugging) {
        try {
            // Load debug extension function pointers
            instance.vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
                vkGetInstanceProcAddr(instance.raw, "vkCreateDebugReportCallbackEXT"));
            instance.vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
                vkGetInstanceProcAddr(instance.raw, "vkDestroyDebugReportCallbackEXT"));
            instance.vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance.raw, "vkCreateDebugUtilsMessengerEXT"));
            instance.vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance.raw, "vkDestroyDebugUtilsMessengerEXT"));

            // Create debug report callback
            VkDebugReportCallbackCreateInfoEXT debugInfo{};
            debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                            VK_DEBUG_REPORT_WARNING_BIT_EXT |
                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debugInfo.pfnCallback = &Instance::VulkanDebugCallback;

            if (instance.vkCreateDebugReportCallbackEXT) {
                result = instance.vkCreateDebugReportCallbackEXT(instance.raw, &debugInfo, nullptr, &instance.debugCallback);
                if (result != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create debug report callback");
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to setup debug reporting: ") + e.what());
        }
    }

    return instance;
}

std::vector<const char*> Instance::ExtensionNames(const DeviceBuilder& builder) {
    std::vector<const char*> names;
    
    // Always include VK_KHR_get_physical_device_properties2
    names.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    
    if (builder.graphicsDebugging) {
        names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return names;
}

std::vector<const char*> Instance::LayerNames(const DeviceBuilder& builder) {
    std::vector<const char*> layerNames;

    if (builder.graphicsDebugging) {
        layerNames.push_back("VK_LAYER_KHRONOS_validation");
    }

    return layerNames;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Instance::VulkanDebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t srcObj,
    size_t location,
    int32_t msgCode,
    const char* layerPrefix,
    const char* message,
    void* userData) {

    // Suppress unused parameter warnings
    (void)flags;
    (void)objType;
    (void)srcObj;
    (void)location;
    (void)msgCode;
    (void)layerPrefix;
    (void)userData;

    std::string msgStr(message);
    
    if (msgStr.find("Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-00322") == 0 ||
        msgStr.find("Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-02752") == 0) {
        // Validation layers incorrectly report an error in pushing immutable sampler descriptors.
        //
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetKHR.html
        // This documentation claims that it's necessary to push immutable samplers.
    } else if (msgStr.find("Validation Performance Warning") == 0) {
        // Ignore performance warnings
    } else if (msgStr.find("Validation Warning: [ VUID_Undefined ]") == 0) {
        // Log warning messages
        // Note: In C++ you would use your logging system here
    } else {
        // Log error messages
        // Note: In C++ you would use your logging system here
    }
    
    return VK_FALSE;
}

} // namespace tekki::backend::vulkan