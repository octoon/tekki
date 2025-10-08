// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/instance.rs

#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <string>

#include "core/common.h"

namespace tekki::backend::vulkan {

class InstanceBuilder {
public:
    InstanceBuilder();

    InstanceBuilder& required_extensions(std::vector<const char*> extensions);
    InstanceBuilder& graphics_debugging(bool enable);

    std::shared_ptr<class Instance> build();

private:
    std::vector<const char*> required_extensions_;
    bool graphics_debugging_{false};
};

class Instance {
public:
    static InstanceBuilder builder();

    ~Instance();

    // Getters
    vk::Instance raw() const { return instance_; }
    vk::EntryLoader& entry() { return entry_; }

private:
    friend class InstanceBuilder;
    Instance() = default;

    static std::unique_ptr<Instance> create(const InstanceBuilder& builder);

    static std::vector<const char*> extension_names(const InstanceBuilder& builder);
    static std::vector<std::string> layer_names(const InstanceBuilder& builder);

    vk::EntryLoader entry_;
    vk::Instance instance_;

    // Debug utilities
    vk::DebugReportCallbackEXT debug_callback_{VK_NULL_HANDLE};
    vk::DebugUtilsMessengerEXT debug_utils_messenger_{VK_NULL_HANDLE};
    std::unique_ptr<vk::DispatchLoaderDynamic> debug_utils_;
};

// Debug callback function
VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT obj_type,
    uint64_t src_obj,
    size_t location,
    int32_t msg_code,
    const char* layer_prefix,
    const char* message,
    void* user_data
);

} // namespace tekki::backend::vulkan