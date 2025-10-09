// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/physical_device.rs

#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "queue_family.h"
#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

class Instance;
class Surface;

/**
 * @brief Physical device wrapper
 */
class PhysicalDevice
{
public:
    PhysicalDevice(const std::shared_ptr<Instance>& instance, vk::PhysicalDevice raw,
                   std::vector<QueueFamily> queue_families, bool presentation_requested);

    // Accessors
    vk::PhysicalDevice raw() const { return raw_; }
    const std::vector<QueueFamily>& queue_families() const { return queue_families_; }
    bool presentation_requested() const { return presentation_requested_; }
    const vk::PhysicalDeviceProperties& properties() const { return properties_; }
    const vk::PhysicalDeviceMemoryProperties& memory_properties() const { return memory_properties_; }
    const std::shared_ptr<Instance>& instance() const { return instance_; }

    /**
     * @brief Find a queue family that supports the given flags
     */
    std::optional<QueueFamily> find_queue_family(vk::QueueFlags flags) const;

private:
    std::shared_ptr<Instance> instance_;
    vk::PhysicalDevice raw_;
    std::vector<QueueFamily> queue_families_;
    bool presentation_requested_;
    vk::PhysicalDeviceProperties properties_;
    vk::PhysicalDeviceMemoryProperties memory_properties_;

    friend class Device;
};

/**
 * @brief Enumerate all physical devices
 */
std::vector<std::shared_ptr<PhysicalDevice>> EnumeratePhysicalDevices(const std::shared_ptr<Instance>& instance);

/**
 * @brief Filter physical devices by presentation support
 */
std::vector<std::shared_ptr<PhysicalDevice>>
FilterPhysicalDevicesWithPresentationSupport(std::vector<std::shared_ptr<PhysicalDevice>> devices,
                                              const std::shared_ptr<Surface>& surface);

} // namespace tekki::backend::vulkan
