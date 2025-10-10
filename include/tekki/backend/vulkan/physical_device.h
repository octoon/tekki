#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include "instance.h"
#include "surface.h"
#include "tekki/core/result.h"

namespace tekki::backend::vulkan {

struct QueueFamily {
    uint32_t index;
    VkQueueFamilyProperties properties;
};

class PhysicalDevice {
public:
    std::shared_ptr<Instance> instance;
    VkPhysicalDevice raw;
    std::vector<QueueFamily> queueFamilies;
    bool presentationRequested;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    PhysicalDevice(
        std::shared_ptr<Instance> instance,
        VkPhysicalDevice raw,
        std::vector<QueueFamily> queueFamilies,
        bool presentationRequested,
        VkPhysicalDeviceProperties properties,
        VkPhysicalDeviceMemoryProperties memoryProperties
    ) : instance(instance),
        raw(raw),
        queueFamilies(queueFamilies),
        presentationRequested(presentationRequested),
        properties(properties),
        memoryProperties(memoryProperties) {}

    friend std::ostream& operator<<(std::ostream& os, const PhysicalDevice& device);
};

std::vector<PhysicalDevice> EnumeratePhysicalDevices(std::shared_ptr<Instance> instance);

class PhysicalDeviceList {
public:
    static std::vector<PhysicalDevice> WithPresentationSupport(std::vector<PhysicalDevice> devices, std::shared_ptr<Surface> surface);
};

} // namespace tekki::backend::vulkan