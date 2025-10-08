// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/instance.rs

#include "../../include/tekki/backend/vulkan/instance.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

namespace tekki::backend::vulkan
{

InstanceBuilder::InstanceBuilder()
{
    // Default extensions
    required_extensions_.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

InstanceBuilder& InstanceBuilder::required_extensions(std::vector<const char*> extensions)
{
    required_extensions_.insert(required_extensions_.end(), extensions.begin(), extensions.end());
    return *this;
}

InstanceBuilder& InstanceBuilder::graphics_debugging(bool enable)
{
    graphics_debugging_ = enable;
    return *this;
}

std::shared_ptr<Instance> InstanceBuilder::build()
{
    return Instance::create(*this);
}

// Static methods
InstanceBuilder Instance::builder()
{
    return InstanceBuilder{};
}

std::unique_ptr<Instance> Instance::create(const InstanceBuilder& builder)
{
    auto instance = std::make_unique<Instance>();

    // Create Vulkan instance
    vk::ApplicationInfo app_info{.pApplicationName = "tekki",
                                 .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                 .pEngineName = "tekki",
                                 .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                 .apiVersion = VK_API_VERSION_1_2};

    auto extension_names = Instance::extension_names(builder);
    auto layer_names = Instance::layer_names(builder);

    vk::InstanceCreateInfo create_info{.pApplicationInfo = &app_info,
                                       .enabledLayerCount = static_cast<uint32_t>(layer_names.size()),
                                       .ppEnabledLayerNames = layer_names.data(),
                                       .enabledExtensionCount = static_cast<uint32_t>(extension_names.size()),
                                       .ppEnabledExtensionNames = extension_names.data()};

    // Create instance
    instance->instance_ = vk::createInstance(create_info);
    spdlog::info("Created a Vulkan instance");

    // Setup debug utilities if enabled
    if (builder.graphics_debugging_)
    {
        vk::DebugReportCallbackCreateInfoEXT debug_info{.flags = vk::DebugReportFlagBitsEXT::eError |
                                                                 vk::DebugReportFlagBitsEXT::eWarning |
                                                                 vk::DebugReportFlagBitsEXT::ePerformanceWarning,
                                                        .pfnCallback = vulkan_debug_callback};

        // Create debug callback
        instance->debug_callback_ = instance->instance_.createDebugReportCallbackEXT(debug_info);

        // Setup debug utils
        instance->debug_utils_ =
            std::make_unique<vk::DispatchLoaderDynamic>(instance->instance_, vkGetInstanceProcAddr);
    }

    return instance;
}

std::vector<const char*> Instance::extension_names(const InstanceBuilder& builder)
{
    std::vector<const char*> names{VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};

    if (builder.graphics_debugging_)
    {
        names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Add user-requested extensions
    names.insert(names.end(), builder.required_extensions_.begin(), builder.required_extensions_.end());

    return names;
}

std::vector<std::string> Instance::layer_names(const InstanceBuilder& builder)
{
    std::vector<std::string> layer_names;

    if (builder.graphics_debugging_)
    {
        layer_names.push_back("VK_LAYER_KHRONOS_validation");
    }

    return layer_names;
}

Instance::~Instance()
{
    if (debug_callback_)
    {
        instance_.destroyDebugReportCallbackEXT(debug_callback_);
    }

    if (instance_)
    {
        instance_.destroy();
    }
}

// Debug callback implementation
VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT obj_type,
                                                     uint64_t src_obj, size_t location, int32_t msg_code,
                                                     const char* layer_prefix, const char* message, void* user_data)
{
    std::string msg_str(message);

    // Filter out known false positives
    if (msg_str.starts_with("Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-00322") ||
        msg_str.starts_with("Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-02752"))
    {
        // Validation layers incorrectly report an error in pushing immutable sampler descriptors
        return VK_FALSE;
    }
    else if (msg_str.starts_with("Validation Performance Warning"))
    {
        // Ignore performance warnings
        return VK_FALSE;
    }
    else if (msg_str.starts_with("Validation Warning: [ VUID_Undefined ]"))
    {
        spdlog::warn("{}", message);
    }
    else
    {
        spdlog::error("{}", message);
    }

    return VK_FALSE;
}

} // namespace tekki::backend::vulkan