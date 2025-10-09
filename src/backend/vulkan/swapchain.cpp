#include "tekki/backend/vulkan/swapchain.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace tekki::backend::vulkan {

std::vector<VkSurfaceFormatKHR> Swapchain::EnumerateSurfaceFormats(
    const std::shared_ptr<Device>& device,
    const Surface& surface
) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device->GetPhysicalDevice(),
        surface.GetRaw(),
        &formatCount,
        nullptr
    );

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device->GetPhysicalDevice(),
        surface.GetRaw(),
        &formatCount,
        formats.data()
    );

    return formats;
}

Swapchain::Swapchain(
    const std::shared_ptr<Device>& device,
    const std::shared_ptr<Surface>& surface,
    const SwapchainDesc& desc
) : Device(device), SurfaceRef(surface), Desc(desc), NextSemaphore(0) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device->GetPhysicalDevice(),
        surface->GetRaw(),
        &surfaceCapabilities
    );

    Raw = CreateSwapchain(device, surface, desc, surfaceCapabilities);
    auto vkImages = GetSwapchainImages(Raw);
    
    Images.reserve(vkImages.size());
    for (auto vkImage : vkImages) {
        ImageDesc imageDesc{
            ImageType::Tex2d,
            VK_IMAGE_USAGE_STORAGE_BIT,
            0,
            VK_FORMAT_B8G8R8A8_UNORM,
            {desc.Dims.width, desc.Dims.height, 0},
            VK_IMAGE_TILING_OPTIMAL,
            1,
            1
        };
        Images.push_back(std::make_shared<Image>(vkImage, imageDesc));
    }

    AcquireSemaphores = CreateSemaphores(device, Images.size());
    RenderingFinishedSemaphores = CreateSemaphores(device, Images.size());
}

Swapchain::~Swapchain() {
    for (auto semaphore : AcquireSemaphores) {
        vkDestroySemaphore(Device->GetRaw(), semaphore, nullptr);
    }
    for (auto semaphore : RenderingFinishedSemaphores) {
        vkDestroySemaphore(Device->GetRaw(), semaphore, nullptr);
    }
    if (Raw != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(Device->GetRaw(), Raw, nullptr);
    }
}

glm::uvec2 Swapchain::Extent() const {
    return glm::uvec2(Desc.Dims.width, Desc.Dims.height);
}

SwapchainImage Swapchain::AcquireNextImage() {
    VkSemaphore acquireSemaphore = AcquireSemaphores[NextSemaphore];
    VkSemaphore renderingFinishedSemaphore = RenderingFinishedSemaphores[NextSemaphore];

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        Device->GetRaw(),
        Raw,
        UINT64_MAX,
        acquireSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        throw SwapchainAcquireImageErr::RecreateFramebuffer;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not acquire swapchain image");
    }

    if (imageIndex != NextSemaphore) {
        throw std::runtime_error("Swapchain image index mismatch");
    }

    NextSemaphore = (NextSemaphore + 1) % Images.size();

    return SwapchainImage{
        Images[imageIndex],
        imageIndex,
        acquireSemaphore,
        renderingFinishedSemaphore
    };
}

void Swapchain::PresentImage(const SwapchainImage& image) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &image.RenderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &Raw;
    presentInfo.pImageIndices = &image.ImageIndex;

    VkResult result = vkQueuePresentKHR(Device->GetUniversalQueue(), &presentInfo);
    
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Could not present image");
    }
}

VkSwapchainKHR Swapchain::CreateSwapchain(
    const std::shared_ptr<Device>& device,
    const std::shared_ptr<Surface>& surface,
    const SwapchainDesc& desc,
    VkSurfaceCapabilitiesKHR surfaceCapabilities
) {
    uint32_t desiredImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if (surfaceCapabilities.maxImageCount > 0) {
        desiredImageCount = std::min(desiredImageCount, surfaceCapabilities.maxImageCount);
    }

    VkExtent2D surfaceResolution;
    if (surfaceCapabilities.currentExtent.width == UINT32_MAX) {
        surfaceResolution = desc.Dims;
    } else {
        surfaceResolution = surfaceCapabilities.currentExtent;
    }

    if (surfaceResolution.width == 0 || surfaceResolution.height == 0) {
        throw std::runtime_error("Swapchain resolution cannot be zero");
    }

    std::vector<VkPresentModeKHR> presentModePreference;
    if (desc.Vsync) {
        presentModePreference = {VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR};
    } else {
        presentModePreference = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device->GetPhysicalDevice(),
        surface->GetRaw(),
        &presentModeCount,
        nullptr
    );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device->GetPhysicalDevice(),
        surface->GetRaw(),
        &presentModeCount,
        presentModes.data()
    );

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto preferredMode : presentModePreference) {
        if (std::find(presentModes.begin(), presentModes.end(), preferredMode) != presentModes.end()) {
            presentMode = preferredMode;
            break;
        }
    }

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface->GetRaw();
    createInfo.minImageCount = desiredImageCount;
    createInfo.imageFormat = desc.Format.format;
    createInfo.imageColorSpace = desc.Format.colorSpace;
    createInfo.imageExtent = surfaceResolution;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = preTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device->GetRaw(), &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    return swapchain;
}

std::vector<VkImage> Swapchain::GetSwapchainImages(VkSwapchainKHR swapchain) {
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(Device->GetRaw(), swapchain, &imageCount, nullptr);
    
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(Device->GetRaw(), swapchain, &imageCount, images.data());
    
    return images;
}

std::vector<VkSemaphore> Swapchain::CreateSemaphores(const std::shared_ptr<Device>& device, size_t count) {
    std::vector<VkSemaphore> semaphores;
    semaphores.reserve(count);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    for (size_t i = 0; i < count; ++i) {
        VkSemaphore semaphore;
        if (vkCreateSemaphore(device->GetRaw(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore");
        }
        semaphores.push_back(semaphore);
    }
    
    return semaphores;
}

} // namespace tekki::backend::vulkan