#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace tekki::backend::vulkan {

class Instance;

class Surface {
public:
    /**
     * Create a new Surface
     * @param instance The Vulkan instance
     * @param window The window to create surface for
     * @return Shared pointer to the created Surface
     */
    static std::shared_ptr<Surface> Create(
        const std::shared_ptr<Instance>& instance,
        void* window
    );

    VkSurfaceKHR GetRaw() const { return raw; }
    VkSurfaceKHR* GetRawPtr() { return &raw; }
    const VkSurfaceKHR* GetRawPtr() const { return &raw; }

private:
    Surface(VkSurfaceKHR surface, VkInstance instance);

    VkSurfaceKHR raw;
    VkInstance instanceHandle;
};

} // namespace tekki::backend::vulkan