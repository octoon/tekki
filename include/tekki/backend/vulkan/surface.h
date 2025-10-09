// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/surface.rs

#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

#include "tekki/core/common.h"

// Forward declarations for window handle
struct GLFWwindow;

namespace tekki::backend::vulkan
{

class Instance;

/**
 * @brief Vulkan surface wrapper
 */
class Surface
{
public:
    /**
     * @brief Create a surface from a GLFW window
     */
    static std::shared_ptr<Surface> Create(const std::shared_ptr<Instance>& instance, GLFWwindow* window);

    ~Surface();

    // Accessors
    vk::SurfaceKHR raw() const { return surface_; }
    const std::shared_ptr<Instance>& instance() const { return instance_; }

private:
    Surface(const std::shared_ptr<Instance>& instance, vk::SurfaceKHR surface);

    std::shared_ptr<Instance> instance_;
    vk::SurfaceKHR surface_;
};

} // namespace tekki::backend::vulkan
