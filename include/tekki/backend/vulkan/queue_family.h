// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Placeholder implementation for QueueFamily

#pragma once

#include <vulkan/vulkan.hpp>

namespace tekki::backend::vulkan
{

class QueueFamily
{
public:
    uint32_t index;
    vk::QueueFlags flags;

    QueueFamily(uint32_t idx, vk::QueueFlags f) : index(idx), flags(f) {}

    // Placeholder methods
    bool supports_graphics() const { return static_cast<bool>(flags & vk::QueueFlagBits::eGraphics); }
    bool supports_compute() const { return static_cast<bool>(flags & vk::QueueFlagBits::eCompute); }
    bool supports_transfer() const { return static_cast<bool>(flags & vk::QueueFlagBits::eTransfer); }
};

} // namespace tekki::backend::vulkan