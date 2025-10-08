// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/dynamic_constants.rs

#include "backend/vulkan/dynamic_constants.h"

#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan
{

DynamicConstants::DynamicConstants(std::shared_ptr<Buffer> buffer) : buffer_(std::move(buffer)) {}

void DynamicConstants::advance_frame()
{
    frame_parity_ = (frame_parity_ + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT;
    frame_offset_bytes_ = 0;
}

uint32_t DynamicConstants::current_offset() const
{
    return static_cast<uint32_t>(frame_parity_ * DYNAMIC_CONSTANTS_SIZE_BYTES + frame_offset_bytes_);
}

vk::DeviceAddress DynamicConstants::current_device_address(vk::Device device) const
{
    return buffer_->device_address(device) + current_offset();
}

} // namespace tekki::backend::vulkan