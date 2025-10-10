#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "vulkan/vulkan.h"
#include "device.h"
#include "surface.h"
#include "swapchain.h"
#include "physical_device.h"

namespace tekki::backend::vulkan {

class RenderBackend {
public:
    std::shared_ptr<Device> Device;
    std::shared_ptr<Surface> Surface;
    std::shared_ptr<Swapchain> Swapchain;

    struct Config {
        glm::u32vec2 SwapchainExtent;
        bool Vsync;
        bool GraphicsDebugging;
        std::optional<size_t> DeviceIndex;
    };

    static std::shared_ptr<RenderBackend> Create(void* window, const Config& config);

private:
    static VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
};

} // namespace tekki::backend::vulkan