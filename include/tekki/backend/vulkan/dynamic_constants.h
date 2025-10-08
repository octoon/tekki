// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/dynamic_constants.rs

#pragma once

#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

// Constants from Rust original
constexpr size_t DYNAMIC_CONSTANTS_SIZE_BYTES = 1024 * 1024 * 16;
constexpr size_t DYNAMIC_CONSTANTS_BUFFER_COUNT = 2;
constexpr size_t MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH = 16384;
constexpr size_t DYNAMIC_CONSTANTS_ALIGNMENT = 256;
constexpr size_t MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES = 1024 * 1024;

class DynamicConstants
{
public:
    DynamicConstants(std::shared_ptr<Buffer> buffer);

    // Frame management
    void advance_frame();

    // Data management
    template <typename T> uint32_t push(const T& data);

    template <typename T>
    uint32_t push_from_iter(typename std::vector<T>::const_iterator begin, typename std::vector<T>::const_iterator end);

    // Getters
    uint32_t current_offset() const;
    vk::DeviceAddress current_device_address(vk::Device device) const;

private:
    std::shared_ptr<Buffer> buffer_;
    size_t frame_offset_bytes_{0};
    size_t frame_parity_{0};
};

// Template implementation

template <typename T> uint32_t DynamicConstants::push(const T& data)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    const size_t t_size = sizeof(T);
    if (frame_offset_bytes_ + t_size >= DYNAMIC_CONSTANTS_SIZE_BYTES)
    {
        throw std::runtime_error("Dynamic constants buffer overflow");
    }

    const uint32_t buffer_offset = current_offset();

    // TODO: Implement memory mapping and data copying
    // This requires access to the buffer's mapped memory

    // Align the offset
    const size_t t_size_aligned = (t_size + DYNAMIC_CONSTANTS_ALIGNMENT - 1) & ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);
    frame_offset_bytes_ += t_size_aligned;

    return buffer_offset;
}

template <typename T>
uint32_t DynamicConstants::push_from_iter(typename std::vector<T>::const_iterator begin,
                                          typename std::vector<T>::const_iterator end)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    const size_t t_size = sizeof(T);
    const size_t t_align = alignof(T);

    if (frame_offset_bytes_ + t_size >= DYNAMIC_CONSTANTS_SIZE_BYTES)
    {
        throw std::runtime_error("Dynamic constants buffer overflow");
    }

    if (DYNAMIC_CONSTANTS_ALIGNMENT % t_align != 0)
    {
        throw std::runtime_error("Alignment mismatch");
    }

    const uint32_t buffer_offset = current_offset();

    // TODO: Implement memory mapping and data copying for iterator range
    // This requires access to the buffer's mapped memory

    size_t dst_offset = buffer_offset;
    for (auto it = begin; it != end; ++it)
    {
        // Copy each element
        // TODO: Copy data to mapped memory

        dst_offset += t_size + t_align - 1;
        dst_offset &= ~(t_align - 1);
    }

    frame_offset_bytes_ += (dst_offset - buffer_offset);
    frame_offset_bytes_ += DYNAMIC_CONSTANTS_ALIGNMENT - 1;
    frame_offset_bytes_ &= ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);

    return buffer_offset;
}

} // namespace tekki::backend::vulkan