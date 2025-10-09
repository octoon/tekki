// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "../../include/tekki/backend/vulkan/allocator.h"

#include <spdlog/spdlog.h>

#include "../../include/tekki/backend/vulkan/instance.h"
#include "../../include/tekki/backend/vulkan/physical_device.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace tekki::backend::vulkan
{

VulkanAllocator::VulkanAllocator(VmaAllocator allocator) : allocator_(allocator) {}

VulkanAllocator::~VulkanAllocator()
{
    if (allocator_)
    {
        vmaDestroyAllocator(allocator_);
    }
}

std::shared_ptr<VulkanAllocator> VulkanAllocator::Create(const std::shared_ptr<Instance>& instance,
                                                         const std::shared_ptr<PhysicalDevice>& physical_device,
                                                         vk::Device device)
{
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
    allocator_info.instance = instance->raw();
    allocator_info.physicalDevice = physical_device->raw();
    allocator_info.device = device;
    allocator_info.pVulkanFunctions = &vulkan_functions;

    VmaAllocator allocator;
    VkResult result = vmaCreateAllocator(&allocator_info, &allocator);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VMA allocator");
    }

    spdlog::info("Created VMA allocator");
    return std::shared_ptr<VulkanAllocator>(new VulkanAllocator(allocator));
}

} // namespace tekki::backend::vulkan
