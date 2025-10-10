#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <optional>
#include "device.h"

namespace tekki::backend::vulkan {

// Simplified access type enum to replace vk_sync::AccessType
enum class AccessType {
    Nothing,
    VertexShaderReadSampledImageOrUniformTexelBuffer,
    FragmentShaderReadSampledImageOrUniformTexelBuffer,
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    ColorAttachmentRead,
    ColorAttachmentWrite,
    DepthStencilAttachmentRead,
    DepthStencilAttachmentWrite,
    TransferRead,
    TransferWrite,
    General
};

class ImageBarrier {
public:
    ImageBarrier(
        VkImage image,
        AccessType prevAccess,
        AccessType nextAccess,
        VkImageAspectFlags aspectMask
    ) : Image(image), PrevAccess(prevAccess), NextAccess(nextAccess), Discard(false), AspectMask(aspectMask) {}

    ImageBarrier WithDiscard(bool discard) {
        Discard = discard;
        return *this;
    }

    VkImage Image;
    AccessType PrevAccess;
    AccessType NextAccess;
    bool Discard;
    VkImageAspectFlags AspectMask;
};

inline void RecordImageBarrier(std::shared_ptr<Device> /*device*/, VkCommandBuffer commandBuffer, ImageBarrier barrier) {
    VkImageSubresourceRange range = {};
    range.aspectMask = barrier.AspectMask;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    // Convert AccessType to Vulkan layout and access flags
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    // Convert previous access
    switch (barrier.PrevAccess) {
        case AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
            oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            break;
        case AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
            oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
            oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case AccessType::ColorAttachmentRead:
            oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case AccessType::ColorAttachmentWrite:
            oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case AccessType::DepthStencilAttachmentRead:
            oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case AccessType::DepthStencilAttachmentWrite:
            oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case AccessType::TransferRead:
            oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case AccessType::TransferWrite:
            oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case AccessType::General:
            oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            break;
        default:
            oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            srcAccessMask = 0;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
    }

    // Convert next access
    switch (barrier.NextAccess) {
        case AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            break;
        case AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case AccessType::ColorAttachmentRead:
            newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case AccessType::ColorAttachmentWrite:
            newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case AccessType::DepthStencilAttachmentRead:
            newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case AccessType::DepthStencilAttachmentWrite:
            newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case AccessType::TransferRead:
            newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case AccessType::TransferWrite:
            newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case AccessType::General:
            newLayout = VK_IMAGE_LAYOUT_GENERAL;
            dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            break;
        default:
            newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            dstAccessMask = 0;
            dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
    }

    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstAccessMask = dstAccessMask;
    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = barrier.Image;
    imageBarrier.subresourceRange = range;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier
    );
}

struct AccessInfo {
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout ImageLayout;
};

inline AccessInfo GetAccessInfo(AccessType accessType) {
    AccessInfo info = {};

    switch (accessType) {
        case AccessType::Nothing:
            info.StageMask = 0;
            info.AccessMask = 0;
            info.ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
            info.StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            info.AccessMask = VK_ACCESS_SHADER_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
            info.StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            info.AccessMask = VK_ACCESS_SHADER_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
            info.StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            info.AccessMask = VK_ACCESS_SHADER_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case AccessType::ColorAttachmentRead:
            info.StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            info.AccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case AccessType::ColorAttachmentWrite:
            info.StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            info.AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case AccessType::DepthStencilAttachmentRead:
            info.StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            info.AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            break;
        case AccessType::DepthStencilAttachmentWrite:
            info.StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            info.AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case AccessType::TransferRead:
            info.StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            info.AccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case AccessType::TransferWrite:
            info.StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            info.AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        case AccessType::General:
            info.StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            info.AccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            info.ImageLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        default:
            info.StageMask = 0;
            info.AccessMask = 0;
            info.ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }

    return info;
}

inline VkImageAspectFlags ImageAspectMaskFromFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

inline std::optional<VkImageAspectFlags> ImageAspectMaskFromAccessTypeAndFormat(
    AccessType accessType,
    VkFormat format) {
    VkImageLayout imageLayout = GetAccessInfo(accessType).ImageLayout;

    switch (imageLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return ImageAspectMaskFromFormat(format);
        default:
            return std::nullopt;
    }
}

} // namespace tekki::backend::vulkan
