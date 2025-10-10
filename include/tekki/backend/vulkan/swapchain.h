#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "device.h"
#include "surface.h"
#include "image.h"

namespace tekki::backend::vulkan {

struct SwapchainDesc {
    VkSurfaceFormatKHR Format;
    VkExtent2D Dims;
    bool Vsync;

    SwapchainDesc() : Format{}, Dims{}, Vsync(false) {}
};

struct SwapchainImage {
    std::shared_ptr<Image> Image;
    uint32_t ImageIndex;
    VkSemaphore AcquireSemaphore;
    VkSemaphore RenderingFinishedSemaphore;
};

enum class SwapchainAcquireImageErr {
    RecreateFramebuffer
};

class Swapchain {
public:
    static std::vector<VkSurfaceFormatKHR> EnumerateSurfaceFormats(
        const std::shared_ptr<Device>& device,
        const Surface& surface
    );

    Swapchain(
        const std::shared_ptr<Device>& device,
        const std::shared_ptr<Surface>& surface,
        const SwapchainDesc& desc
    );

    ~Swapchain();

    glm::uvec2 Extent() const;

    SwapchainImage AcquireNextImage();

    void PresentImage(const SwapchainImage& image);

    VkSwapchainKHR GetRaw() const { return Raw; }
    const SwapchainDesc& GetDesc() const { return Desc; }
    const std::vector<std::shared_ptr<Image>>& GetImages() const { return Images; }
    const std::vector<VkSemaphore>& GetAcquireSemaphores() const { return AcquireSemaphores; }
    const std::vector<VkSemaphore>& GetRenderingFinishedSemaphores() const { return RenderingFinishedSemaphores; }

private:
    VkSwapchainKHR Raw;
    SwapchainDesc Desc;
    std::vector<std::shared_ptr<Image>> Images;
    std::vector<VkSemaphore> AcquireSemaphores;
    std::vector<VkSemaphore> RenderingFinishedSemaphores;
    size_t NextSemaphore;

    std::shared_ptr<Device> device_;
    std::shared_ptr<Surface> surfaceRef_;

    VkSwapchainKHR CreateSwapchain(
        const std::shared_ptr<Device>& device,
        const std::shared_ptr<Surface>& surface,
        const SwapchainDesc& desc,
        VkSurfaceCapabilitiesKHR surfaceCapabilities
    );

    std::vector<VkImage> GetSwapchainImages(VkSwapchainKHR swapchain);
    std::vector<VkSemaphore> CreateSemaphores(const std::shared_ptr<Device>& device, size_t count);
};

} // namespace tekki::backend::vulkan