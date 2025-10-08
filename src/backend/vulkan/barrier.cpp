// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/barrier.rs

#include "../../include/tekki/backend/vulkan/barrier.h"

#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan
{

ImageBarrier::ImageBarrier(vk::Image image, AccessType prev_access, AccessType next_access,
                           vk::ImageAspectFlags aspect_mask)
    : image_(image), prev_access_(prev_access), next_access_(next_access), aspect_mask_(aspect_mask)
{
}

ImageBarrier& ImageBarrier::with_discard(bool discard)
{
    discard_ = discard;
    return *this;
}

void ImageBarrier::record(vk::CommandBuffer command_buffer, const class Device& device)
{
    // TODO: Implement barrier recording using vk_sync equivalent
    // This requires a C++ equivalent of the vk_sync library

    spdlog::warn("Image barrier recording not yet implemented");
}

AccessInfo get_access_info(AccessType access_type)
{
    // Simplified implementation - this should match the Rust vk_sync implementation
    switch (access_type)
    {
    case AccessType::Nothing:
        return {vk::PipelineStageFlags{}, vk::AccessFlags{}, vk::ImageLayout::eUndefined};
    case AccessType::IndirectBuffer:
        return {vk::PipelineStageFlagBits::eDrawIndirect, vk::AccessFlagBits::eIndirectCommandRead,
                vk::ImageLayout::eUndefined};
    case AccessType::IndexBuffer:
        return {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eIndexRead, vk::ImageLayout::eUndefined};
    case AccessType::VertexBuffer:
        return {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead,
                vk::ImageLayout::eUndefined};
    case AccessType::VertexShaderReadUniformBuffer:
        return {vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined};
    case AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
        return {vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal};
    case AccessType::FragmentShaderReadUniformBuffer:
        return {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eUniformRead,
                vk::ImageLayout::eUndefined};
    case AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
        return {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal};
    case AccessType::ComputeShaderReadUniformBuffer:
        return {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eUniformRead,
                vk::ImageLayout::eUndefined};
    case AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
        return {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal};
    case AccessType::ColorAttachmentRead:
        return {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentRead,
                vk::ImageLayout::eColorAttachmentOptimal};
    case AccessType::ColorAttachmentWrite:
        return {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite,
                vk::ImageLayout::eColorAttachmentOptimal};
    case AccessType::DepthStencilAttachmentRead:
        return {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                vk::AccessFlagBits::eDepthStencilAttachmentRead, vk::ImageLayout::eDepthStencilReadOnlyOptimal};
    case AccessType::DepthStencilAttachmentWrite:
        return {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::ImageLayout::eDepthStencilAttachmentOptimal};
    case AccessType::TransferRead:
        return {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal};
    case AccessType::TransferWrite:
        return {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eTransferDstOptimal};
    case AccessType::HostRead:
        return {vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostRead, vk::ImageLayout::eGeneral};
    case AccessType::HostWrite:
        return {vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostWrite, vk::ImageLayout::eGeneral};
    case AccessType::Present:
        return {vk::PipelineStageFlags{}, vk::AccessFlags{}, vk::ImageLayout::ePresentSrcKHR};
    case AccessType::General:
        return {vk::PipelineStageFlagBits::eAllCommands,
                vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite, vk::ImageLayout::eGeneral};
    default:
        spdlog::warn("Unimplemented access type: {}", static_cast<int>(access_type));
        return {vk::PipelineStageFlags{}, vk::AccessFlags{}, vk::ImageLayout::eUndefined};
    }
}

vk::ImageAspectFlags image_aspect_mask_from_format(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eD32Sfloat:
        return vk::ImageAspectFlagBits::eDepth;
    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

std::optional<vk::ImageAspectFlags> image_aspect_mask_from_access_type_and_format(AccessType access_type,
                                                                                  vk::Format format)
{
    auto info = get_access_info(access_type);

    switch (info.image_layout)
    {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::eColorAttachmentOptimal:
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
    case vk::ImageLayout::eShaderReadOnlyOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
    case vk::ImageLayout::eTransferDstOptimal:
        return image_aspect_mask_from_format(format);
    default:
        return std::nullopt;
    }
}

} // namespace tekki::backend::vulkan