// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/buffer.rs

#pragma once

#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

enum class MemoryLocation
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu
};

struct BufferDesc
{
    size_t size;
    vk::BufferUsageFlags usage;
    MemoryLocation memory_location;
    std::optional<uint64_t> alignment;

    BufferDesc() = default;

    static BufferDesc new_gpu_only(size_t size, vk::BufferUsageFlags usage)
    {
        BufferDesc desc;
        desc.size = size;
        desc.usage = usage;
        desc.memory_location = MemoryLocation::GpuOnly;
        desc.alignment = std::nullopt;
        return desc;
    }

    static BufferDesc new_cpu_to_gpu(size_t size, vk::BufferUsageFlags usage)
    {
        BufferDesc desc;
        desc.size = size;
        desc.usage = usage;
        desc.memory_location = MemoryLocation::CpuToGpu;
        desc.alignment = std::nullopt;
        return desc;
    }

    static BufferDesc new_gpu_to_cpu(size_t size, vk::BufferUsageFlags usage)
    {
        BufferDesc desc;
        desc.size = size;
        desc.usage = usage;
        desc.memory_location = MemoryLocation::GpuToCpu;
        desc.alignment = std::nullopt;
        return desc;
    }

    BufferDesc& set_alignment(uint64_t align)
    {
        this->alignment = align;
        return *this;
    }

    bool operator==(const BufferDesc& other) const
    {
        return size == other.size && usage == other.usage && memory_location == other.memory_location &&
               alignment == other.alignment;
    }
};

class VulkanAllocator;

class Buffer
{
public:
    Buffer(vk::Buffer buffer, const BufferDesc& desc, VmaAllocation allocation, VmaAllocator allocator_handle);
    ~Buffer();

    // Getters
    vk::Buffer raw() const { return buffer_; }
    const BufferDesc& desc() const { return desc_; }
    VmaAllocation allocation() const { return allocation_; }
    uint64_t device_address(vk::Device device) const;

    // Memory mapping
    void* mapped_data() const;
    size_t mapped_size() const;

private:
    friend class Device;

    vk::Buffer buffer_;
    BufferDesc desc_;
    VmaAllocation allocation_;
    VmaAllocator allocator_handle_; // Store for accessing allocation info
};

} // namespace tekki::backend::vulkan