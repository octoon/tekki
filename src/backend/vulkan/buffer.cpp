// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/buffer.rs

#include "../../include/tekki/backend/vulkan/buffer.h"

#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan
{

Buffer::Buffer(vk::Buffer buffer, const BufferDesc& desc, class SubAllocation allocation)
    : buffer_(buffer), desc_(desc), allocation_(std::move(allocation))
{
}

Buffer::~Buffer()
{
    // Buffer destruction should be handled by Device
}

uint64_t Buffer::device_address(vk::Device device) const
{
    vk::BufferDeviceAddressInfo info{.buffer = buffer_};
    return device.getBufferAddress(info);
}

void* Buffer::mapped_data() const
{
    // TODO: Return mapped data from allocation
    return nullptr;
}

size_t Buffer::mapped_size() const
{
    return desc_.size;
}

} // namespace tekki::backend::vulkan