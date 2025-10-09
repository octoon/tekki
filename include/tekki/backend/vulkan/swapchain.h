// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/swapchain.rs

#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

class Device;
class Surface;
class Image;

/**
 * @brief Swapchain configuration descriptor
 */
struct SwapchainDesc
{
    vk::SurfaceFormatKHR format;
    vk::Extent2D dims;
    bool vsync{false};

    bool operator==(const SwapchainDesc& other) const
    {
        return format == other.format && dims.width == other.dims.width && dims.height == other.dims.height &&
               vsync == other.vsync;
    }
};

/**
 * @brief Acquired swapchain image with synchronization primitives
 */
struct SwapchainImage
{
    std::shared_ptr<Image> image;
    uint32_t image_index;
    vk::Semaphore acquire_semaphore;
    vk::Semaphore rendering_finished_semaphore;
};

/**
 * @brief Error type for swapchain image acquisition
 */
enum class SwapchainAcquireImageError
{
    RecreateFramebuffer
};

/**
 * @brief Vulkan swapchain wrapper
 */
class Swapchain
{
public:
    /**
     * @brief Enumerate available surface formats
     */
    static std::vector<vk::SurfaceFormatKHR> EnumerateSurfaceFormats(const std::shared_ptr<Device>& device,
                                                                      const std::shared_ptr<Surface>& surface);

    /**
     * @brief Create a new swapchain
     */
    static std::shared_ptr<Swapchain> Create(const std::shared_ptr<Device>& device,
                                             const std::shared_ptr<Surface>& surface, const SwapchainDesc& desc);

    ~Swapchain();

    // Accessors
    vk::SwapchainKHR raw() const { return swapchain_; }
    const SwapchainDesc& desc() const { return desc_; }
    const std::vector<std::shared_ptr<Image>>& images() const { return images_; }
    std::array<uint32_t, 2> extent() const { return {desc_.dims.width, desc_.dims.height}; }

    /**
     * @brief Acquire the next swapchain image
     */
    std::expected<SwapchainImage, SwapchainAcquireImageError> AcquireNextImage();

    /**
     * @brief Present the swapchain image
     */
    void PresentImage(const SwapchainImage& image);

private:
    Swapchain(const std::shared_ptr<Device>& device, const std::shared_ptr<Surface>& surface,
              const SwapchainDesc& desc, vk::SwapchainKHR swapchain, std::vector<std::shared_ptr<Image>> images,
              std::vector<vk::Semaphore> acquire_semaphores, std::vector<vk::Semaphore> rendering_finished_semaphores);

    std::shared_ptr<Device> device_;
    std::shared_ptr<Surface> surface_;
    SwapchainDesc desc_;
    vk::SwapchainKHR swapchain_;
    std::vector<std::shared_ptr<Image>> images_;
    std::vector<vk::Semaphore> acquire_semaphores_;
    std::vector<vk::Semaphore> rendering_finished_semaphores_;
    size_t next_semaphore_{0};
};

} // namespace tekki::backend::vulkan
