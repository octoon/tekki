// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/buffer.rs

#pragma once

#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>

#include "core/common.h"

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
        return BufferDesc{
            .size = size, .usage = usage, .memory_location = MemoryLocation::GpuOnly, .alignment = std::nullopt};
    }

    static BufferDesc new_cpu_to_gpu(size_t size, vk::BufferUsageFlags usage)
    {
        return BufferDesc{
            .size = size, .usage = usage, .memory_location = MemoryLocation::CpuToGpu, .alignment = std::nullopt};
    }

    static BufferDesc new_gpu_to_cpu(size_t size, vk::BufferUsageFlags usage)
    {
        return BufferDesc{
            .size = size, .usage = usage, .memory_location = MemoryLocation::GpuToCpu, .alignment = std::nullopt};
    }

    BufferDesc& alignment(uint64_t alignment)
    {
        this->alignment = alignment;
        return *this;
    }

    bool operator==(const BufferDesc& other) const
    {
        return size == other.size && usage == other.usage && memory_location == other.memory_location &&
               alignment == other.alignment;
    }
};

class Buffer
{
public:
    Buffer(vk::Buffer buffer, const BufferDesc& desc, class SubAllocation allocation);
    ~Buffer();

    // Getters
    vk::Buffer raw() const { return buffer_; }
    const BufferDesc& desc() const { return desc_; }
    uint64_t device_address(vk::Device device) const;

    // Memory mapping
    void* mapped_data() const;
    size_t mapped_size() const;

private:
    vk::Buffer buffer_;
    BufferDesc desc_;
    class SubAllocation allocation_;
};

} // namespace tekki::backend::vulkan