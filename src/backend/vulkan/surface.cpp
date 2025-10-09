// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/surface.rs

#include "../../include/tekki/backend/vulkan/surface.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "../../include/tekki/backend/vulkan/instance.h"

namespace tekki::backend::vulkan
{

Surface::Surface(const std::shared_ptr<Instance>& instance, vk::SurfaceKHR surface)
    : instance_(instance), surface_(surface)
{
}

Surface::~Surface()
{
    if (surface_)
    {
        instance_->raw().destroySurfaceKHR(surface_);
    }
}

std::shared_ptr<Surface> Surface::Create(const std::shared_ptr<Instance>& instance, GLFWwindow* window)
{
    VkSurfaceKHR surface_raw;
    VkResult result = glfwCreateWindowSurface(instance->raw(), window, nullptr, &surface_raw);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface");
    }

    vk::SurfaceKHR surface(surface_raw);
    spdlog::info("Created Vulkan surface");

    return std::shared_ptr<Surface>(new Surface(instance, surface));
}

} // namespace tekki::backend::vulkan
