// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <memory>
#include <mutex>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

class Instance;
class PhysicalDevice;

/**
 * @brief Wrapper around VMA (Vulkan Memory Allocator)
 */
class VulkanAllocator
{
public:
    static std::shared_ptr<VulkanAllocator> Create(const std::shared_ptr<Instance>& instance,
                                                   const std::shared_ptr<PhysicalDevice>& physical_device,
                                                   vk::Device device);

    ~VulkanAllocator();

    VmaAllocator raw() const { return allocator_; }

private:
    VulkanAllocator(VmaAllocator allocator);

    VmaAllocator allocator_;
};

} // namespace tekki::backend::vulkan
