// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/swapchain.rs

#include "../../include/tekki/backend/vulkan/swapchain.h"

#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

#include "../../include/tekki/backend/vulkan/device.h"
#include "../../include/tekki/backend/vulkan/image.h"
#include "../../include/tekki/backend/vulkan/physical_device.h"
#include "../../include/tekki/backend/vulkan/surface.h"

namespace tekki::backend::vulkan
{

Swapchain::Swapchain(const std::shared_ptr<Device>& device, const std::shared_ptr<Surface>& surface,
                     const SwapchainDesc& desc, vk::SwapchainKHR swapchain, std::vector<std::shared_ptr<Image>> images,
                     std::vector<vk::Semaphore> acquire_semaphores,
                     std::vector<vk::Semaphore> rendering_finished_semaphores)
    : device_(device), surface_(surface), desc_(desc), swapchain_(swapchain), images_(std::move(images)),
      acquire_semaphores_(std::move(acquire_semaphores)),
      rendering_finished_semaphores_(std::move(rendering_finished_semaphores))
{
}

Swapchain::~Swapchain()
{
    if (swapchain_)
    {
        // Destroy semaphores
        for (auto semaphore : acquire_semaphores_)
        {
            device_->raw().destroySemaphore(semaphore);
        }
        for (auto semaphore : rendering_finished_semaphores_)
        {
            device_->raw().destroySemaphore(semaphore);
        }

        // Destroy swapchain
        device_->raw().destroySwapchainKHR(swapchain_);
    }
}

std::vector<vk::SurfaceFormatKHR> Swapchain::EnumerateSurfaceFormats(const std::shared_ptr<Device>& device,
                                                                      const std::shared_ptr<Surface>& surface)
{
    return device->physical_device()->raw().getSurfaceFormatsKHR(surface->raw());
}

std::shared_ptr<Swapchain> Swapchain::Create(const std::shared_ptr<Device>& device,
                                             const std::shared_ptr<Surface>& surface, const SwapchainDesc& desc)
{
    auto pdevice = device->physical_device();

    // Get surface capabilities
    vk::SurfaceCapabilitiesKHR surface_capabilities = pdevice->raw().getSurfaceCapabilitiesKHR(surface->raw());

    // Triple-buffer so that acquiring an image doesn't stall for >16.6ms at 60Hz on AMD
    // when frames take >16.6ms to render. Also allows MAILBOX to work.
    uint32_t desired_image_count = std::max(3u, surface_capabilities.minImageCount);

    if (surface_capabilities.maxImageCount != 0)
    {
        desired_image_count = std::min(desired_image_count, surface_capabilities.maxImageCount);
    }

    spdlog::info("Swapchain image count: {}", desired_image_count);

    // Determine surface resolution
    vk::Extent2D surface_resolution;
    if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
    {
        surface_resolution = desc.dims;
    }
    else
    {
        surface_resolution = surface_capabilities.currentExtent;
    }

    if (surface_resolution.width == 0 || surface_resolution.height == 0)
    {
        throw std::runtime_error("Swapchain resolution cannot be zero");
    }

    // Select present mode
    std::vector<vk::PresentModeKHR> present_mode_preference;
    if (desc.vsync)
    {
        present_mode_preference = {vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo};
    }
    else
    {
        present_mode_preference = {vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate};
    }

    std::vector<vk::PresentModeKHR> present_modes = pdevice->raw().getSurfacePresentModesKHR(surface->raw());

    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo; // Fallback
    for (auto mode : present_mode_preference)
    {
        if (std::find(present_modes.begin(), present_modes.end(), mode) != present_modes.end())
        {
            present_mode = mode;
            break;
        }
    }

    spdlog::info("Presentation mode: {}", vk::to_string(present_mode));

    // Determine pre-transform
    vk::SurfaceTransformFlagBitsKHR pre_transform;
    if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
    {
        pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    }
    else
    {
        pre_transform = surface_capabilities.currentTransform;
    }

    // Create swapchain
    vk::SwapchainCreateInfoKHR swapchain_info{
        .surface = surface->raw(),
        .minImageCount = desired_image_count,
        .imageFormat = desc.format.format,
        .imageColorSpace = desc.format.colorSpace,
        .imageExtent = surface_resolution,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eStorage,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = pre_transform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr};

    vk::SwapchainKHR swapchain = device->raw().createSwapchainKHR(swapchain_info);

    // Get swapchain images
    std::vector<vk::Image> vk_images = device->raw().getSwapchainImagesKHR(swapchain);

    // Wrap images
    std::vector<std::shared_ptr<Image>> images;
    images.reserve(vk_images.size());

    for (auto vk_image : vk_images)
    {
        ImageDesc image_desc{.extent = vk::Extent3D(desc.dims.width, desc.dims.height, 1),
                             .format = vk::Format::eB8G8R8A8Unorm,
                             .usage = vk::ImageUsageFlagBits::eStorage,
                             .image_type = vk::ImageType::e2D,
                             .mip_levels = 1,
                             .array_layers = 1,
                             .samples = vk::SampleCountFlagBits::e1,
                             .tiling = vk::ImageTiling::eOptimal,
                             .initial_layout = vk::ImageLayout::eUndefined};

        auto image = std::make_shared<Image>(vk_image, image_desc);
        images.push_back(image);
    }

    if (images.size() != desired_image_count)
    {
        spdlog::warn("Requested {} swapchain images, got {}", desired_image_count, images.size());
    }

    // Create semaphores
    std::vector<vk::Semaphore> acquire_semaphores;
    std::vector<vk::Semaphore> rendering_finished_semaphores;

    vk::SemaphoreCreateInfo semaphore_info{};

    for (size_t i = 0; i < images.size(); ++i)
    {
        acquire_semaphores.push_back(device->raw().createSemaphore(semaphore_info));
        rendering_finished_semaphores.push_back(device->raw().createSemaphore(semaphore_info));
    }

    spdlog::info("Created swapchain with {} images ({}x{})", images.size(), surface_resolution.width,
                 surface_resolution.height);

    return std::shared_ptr<Swapchain>(new Swapchain(device, surface, desc, swapchain, std::move(images),
                                                    std::move(acquire_semaphores),
                                                    std::move(rendering_finished_semaphores)));
}

std::expected<SwapchainImage, SwapchainAcquireImageError> Swapchain::AcquireNextImage()
{
    vk::Semaphore acquire_semaphore = acquire_semaphores_[next_semaphore_];
    vk::Semaphore rendering_finished_semaphore = rendering_finished_semaphores_[next_semaphore_];

    try
    {
        auto [result, image_index] =
            device_->raw().acquireNextImageKHR(swapchain_, std::numeric_limits<uint64_t>::max(), acquire_semaphore,
                                               nullptr);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        {
            return std::unexpected(SwapchainAcquireImageError::RecreateFramebuffer);
        }

        // Verify the image index matches our expectation
        if (image_index != next_semaphore_)
        {
            spdlog::warn("Swapchain image index mismatch: expected {}, got {}", next_semaphore_, image_index);
        }

        next_semaphore_ = (next_semaphore_ + 1) % images_.size();

        return SwapchainImage{.image = images_[image_index],
                              .image_index = image_index,
                              .acquire_semaphore = acquire_semaphore,
                              .rendering_finished_semaphore = rendering_finished_semaphore};
    }
    catch (const vk::OutOfDateKHRError&)
    {
        return std::unexpected(SwapchainAcquireImageError::RecreateFramebuffer);
    }
    catch (const vk::SystemError& e)
    {
        throw std::runtime_error(std::string("Failed to acquire swapchain image: ") + e.what());
    }
}

void Swapchain::PresentImage(const SwapchainImage& image)
{
    vk::PresentInfoKHR present_info{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &image.rendering_finished_semaphore,
                                    .swapchainCount = 1,
                                    .pSwapchains = &swapchain_,
                                    .pImageIndices = &image.image_index};

    try
    {
        auto result = device_->universal_queue().raw.presentKHR(present_info);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        {
            // Handled in the next frame
            return;
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        // Handled in the next frame
    }
    catch (const vk::SystemError& e)
    {
        throw std::runtime_error(std::string("Failed to present image: ") + e.what());
    }
}

} // namespace tekki::backend::vulkan
