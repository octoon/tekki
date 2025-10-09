// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/physical_device.rs

#include "../../include/tekki/backend/vulkan/physical_device.h"

#include <spdlog/spdlog.h>

#include "../../include/tekki/backend/vulkan/instance.h"
#include "../../include/tekki/backend/vulkan/surface.h"

namespace tekki::backend::vulkan
{

PhysicalDevice::PhysicalDevice(const std::shared_ptr<Instance>& instance, vk::PhysicalDevice raw,
                               std::vector<QueueFamily> queue_families, bool presentation_requested)
    : instance_(instance), raw_(raw), queue_families_(std::move(queue_families)),
      presentation_requested_(presentation_requested)
{
    // Get device properties
    properties_ = raw_.getProperties();
    memory_properties_ = raw_.getMemoryProperties();
}

std::optional<QueueFamily> PhysicalDevice::find_queue_family(vk::QueueFlags flags) const
{
    for (const auto& family : queue_families_)
    {
        if ((family.properties.queueFlags & flags) == flags)
        {
            return family;
        }
    }
    return std::nullopt;
}

std::vector<std::shared_ptr<PhysicalDevice>> EnumeratePhysicalDevices(const std::shared_ptr<Instance>& instance)
{
    std::vector<vk::PhysicalDevice> pdevices = instance->raw().enumeratePhysicalDevices();

    std::vector<std::shared_ptr<PhysicalDevice>> result;
    result.reserve(pdevices.size());

    for (auto pdevice : pdevices)
    {
        // Get queue family properties
        std::vector<vk::QueueFamilyProperties> queue_family_props = pdevice.getQueueFamilyProperties();

        std::vector<QueueFamily> queue_families;
        queue_families.reserve(queue_family_props.size());

        for (uint32_t i = 0; i < queue_family_props.size(); ++i)
        {
            queue_families.push_back(QueueFamily{.index = i, .properties = queue_family_props[i]});
        }

        auto physical_device = std::make_shared<PhysicalDevice>(instance, pdevice, std::move(queue_families), true);
        result.push_back(physical_device);

        // Log device info
        spdlog::info("Found physical device: {}", physical_device->properties().deviceName.data());
    }

    return result;
}

std::vector<std::shared_ptr<PhysicalDevice>>
FilterPhysicalDevicesWithPresentationSupport(std::vector<std::shared_ptr<PhysicalDevice>> devices,
                                              const std::shared_ptr<Surface>& surface)
{
    std::vector<std::shared_ptr<PhysicalDevice>> result;

    for (auto& pdevice : devices)
    {
        bool supports_presentation = false;

        // Check if any queue family supports presentation
        for (uint32_t queue_index = 0; queue_index < pdevice->queue_families().size(); ++queue_index)
        {
            const auto& family = pdevice->queue_families()[queue_index];

            // Must support graphics
            if (!(family.properties.queueFlags & vk::QueueFlagBits::eGraphics))
            {
                continue;
            }

            // Check presentation support
            vk::Bool32 present_support = surface->raw().getPhysicalDeviceSurfaceSupportKHR(pdevice->raw(), queue_index);

            if (present_support)
            {
                supports_presentation = true;
                break;
            }
        }

        if (supports_presentation)
        {
            result.push_back(pdevice);
        }
    }

    return result;
}

} // namespace tekki::backend::vulkan
