// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/mod.rs

#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "core/common.hpp"

namespace tekki::backend::vulkan
{

/**
 * @brief Main Vulkan backend interface
 *
 * Translates from Rust's RenderBackend struct
 */
class RenderBackend
{
public:
    struct Config
    {
        std::array<uint32_t, 2> swapchain_extent;
        bool vsync;
        bool graphics_debugging;
        std::optional<size_t> device_index;
    };

    static std::unique_ptr<RenderBackend> create(void* window, // Platform-specific window handle
                                                 const Config& config);

    ~RenderBackend();

    // Getters
    std::shared_ptr<class Device> device() const { return device_; }
    std::shared_ptr<class Surface> surface() const { return surface_; }
    class Swapchain& swapchain() { return swapchain_; }

private:
    RenderBackend() = default;

    std::shared_ptr<class Device> device_;
    std::shared_ptr<class Surface> surface_;
    class Swapchain swapchain_;
};

// Helper function from Rust's select_surface_format
vk::SurfaceFormatKHR select_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats);

} // namespace tekki::backend::vulkan