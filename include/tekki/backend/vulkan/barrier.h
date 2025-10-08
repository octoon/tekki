// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/barrier.rs

#pragma once

#include <vulkan/vulkan.hpp>

#include "core/common.h"

namespace tekki::backend::vulkan {

// Access types from vk_sync (simplified)
enum class AccessType {
    Nothing,
    IndirectBuffer,
    IndexBuffer,
    VertexBuffer,
    VertexShaderReadUniformBuffer,
    VertexShaderReadSampledImageOrUniformTexelBuffer,
    VertexShaderReadOther,
    FragmentShaderReadUniformBuffer,
    FragmentShaderReadSampledImageOrUniformTexelBuffer,
    FragmentShaderReadColorInputAttachment,
    FragmentShaderReadDepthStencilInputAttachment,
    FragmentShaderReadOther,
    ColorAttachmentRead,
    DepthStencilAttachmentRead,
    ComputeShaderReadUniformBuffer,
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    ComputeShaderReadOther,
    AnyShaderReadUniformBuffer,
    AnyShaderReadSampledImageOrUniformTexelBuffer,
    AnyShaderReadOther,
    TransferRead,
    HostRead,
    Present,
    VertexShaderWrite,
    FragmentShaderWrite,
    ColorAttachmentWrite,
    DepthStencilAttachmentWrite,
    ComputeShaderWrite,
    AnyShaderWrite,
    TransferWrite,
    HostWrite,
    ColorAttachmentReadWrite,
    General
};

struct AccessInfo {
    vk::PipelineStageFlags stage_mask;
    vk::AccessFlags access_mask;
    vk::ImageLayout image_layout;
};

class ImageBarrier {
public:
    ImageBarrier(
        vk::Image image,
        AccessType prev_access,
        AccessType next_access,
        vk::ImageAspectFlags aspect_mask
    );

    ImageBarrier& with_discard(bool discard);

    void record(vk::CommandBuffer command_buffer, const class Device& device);

private:
    vk::Image image_;
    AccessType prev_access_;
    AccessType next_access_;
    vk::ImageAspectFlags aspect_mask_;
    bool discard_{false};
};

// Utility functions
AccessInfo get_access_info(AccessType access_type);
vk::ImageAspectFlags image_aspect_mask_from_format(vk::Format format);
std::optional<vk::ImageAspectFlags> image_aspect_mask_from_access_type_and_format(
    AccessType access_type,
    vk::Format format
);

} // namespace tekki::backend::vulkan