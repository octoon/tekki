#include "tekki/backend/vulkan/physical_device.h"
#include <stdexcept>
#include <sstream>
#include <glm/glm.hpp>

namespace tekki::backend::vulkan {

std::ostream& operator<<(std::ostream& os, const PhysicalDevice& device) {
    os << "PhysicalDevice { " << device.properties.deviceName << " }";
    return os;
}

std::vector<PhysicalDevice> EnumeratePhysicalDevices(std::shared_ptr<Instance> instance) {
    try {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance->raw, &deviceCount, nullptr);
        
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance->raw, &deviceCount, physicalDevices.data());

        std::vector<PhysicalDevice> result;
        result.reserve(deviceCount);

        for (auto physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
            
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            std::vector<QueueFamily> queueFamilyInfos;
            queueFamilyInfos.reserve(queueFamilyCount);
            
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                queueFamilyInfos.push_back(QueueFamily{
                    .index = i,
                    .properties = queueFamilies[i]
                });
            }

            VkPhysicalDeviceMemoryProperties memoryProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

            result.emplace_back(
                instance,
                physicalDevice,
                queueFamilyInfos,
                true,
                properties,
                memoryProperties
            );
        }

        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to enumerate physical devices: " + std::string(e.what()));
    }
}

std::vector<PhysicalDevice> PhysicalDeviceList::WithPresentationSupport(std::vector<PhysicalDevice> devices, std::shared_ptr<Surface> surface) {
    std::vector<PhysicalDevice> result;
    
    for (auto& device : devices) {
        device.presentationRequested = true;
        
        bool supportsPresentation = false;
        for (size_t i = 0; i < device.queueFamilies.size(); ++i) {
            const auto& queueFamily = device.queueFamilies[i];
            
            if (queueFamily.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    device.raw,
                    static_cast<uint32_t>(i),
                    surface->raw,
                    &presentSupport
                );
                
                if (presentSupport == VK_TRUE) {
                    supportsPresentation = true;
                    break;
                }
            }
        }
        
        if (supportsPresentation) {
            result.push_back(std::move(device));
        }
    }
    
    return result;
}

} // namespace tekki::backend::vulkan