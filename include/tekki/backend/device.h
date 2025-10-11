#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <glm/glm.hpp>

namespace tekki::backend {

class Device {
public:
    virtual ~Device() = default;

    virtual VkDevice GetVkDevice() const = 0;
    virtual VkPhysicalDevice GetVkPhysicalDevice() const = 0;
    virtual VkQueue GetGraphicsQueue() const = 0;
    virtual uint32_t GetGraphicsQueueFamilyIndex() const = 0;

    // Image creation methods would be implemented here
    // std::shared_ptr<class Image> CreateImage(const ImageDesc& desc);
};

} // namespace tekki::backend