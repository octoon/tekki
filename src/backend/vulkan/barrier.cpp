#include "tekki/backend/vulkan/barrier.h"
#include <stdexcept>
#include <vector>
#include <memory>

namespace tekki::backend::vulkan {

ImageBarrier::ImageBarrier(
    VkImage image,
    vk_sync::AccessType prevAccess,
    vk_sync::AccessType nextAccess,
    VkImageAspectFlags aspectMask
) : Image(image), PrevAccess(prevAccess), NextAccess(nextAccess), Discard(false), AspectMask(aspectMask) {}

ImageBarrier ImageBarrier::WithDiscard(bool discard) {
    Discard = discard;
    return *this;
}

void RecordImageBarrier(std::shared_ptr<Device> device, VkCommandBuffer commandBuffer, ImageBarrier barrier) {
    try {
        VkImageSubresourceRange range = {
            .aspectMask = barrier.AspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        };

        vk_sync::cmd::pipeline_barrier(
            device->Raw.FpV10(),
            commandBuffer,
            nullptr,
            std::vector<vk_sync::MemoryBarrier>{},
            std::vector<vk_sync::ImageBarrier>{
                vk_sync::ImageBarrier{
                    .previous_accesses = {barrier.PrevAccess},
                    .next_accesses = {barrier.NextAccess},
                    .previous_layout = vk_sync::ImageLayout::Optimal,
                    .next_layout = vk_sync::ImageLayout::Optimal,
                    .discard_contents = barrier.Discard,
                    .src_queue_family_index = device->UniversalQueue.Family.Index,
                    .dst_queue_family_index = device->UniversalQueue.Family.Index,
                    .image = barrier.Image,
                    .range = range
                }
            }
        );
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to record image barrier: " + std::string(e.what()));
    }
}

AccessInfo GetAccessInfo(vk_sync::AccessType accessType) {
    switch (accessType) {
        case vk_sync::AccessType::Nothing:
            return AccessInfo{
                .StageMask = 0,
                .AccessMask = 0,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::CommandBufferReadNVX:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                .AccessMask = VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::IndirectBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                .AccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::IndexBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                .AccessMask = VK_ACCESS_INDEX_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::VertexBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                .AccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::VertexShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::VertexShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TessellationControlShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::TessellationControlShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TessellationEvaluationShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::TessellationEvaluationShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::GeometryShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::GeometryShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::FragmentShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::FragmentShaderReadColorInputAttachment:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::FragmentShaderReadDepthStencilInputAttachment:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::FragmentShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::ColorAttachmentRead:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };
        case vk_sync::AccessType::DepthStencilAttachmentRead:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::ComputeShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::ComputeShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::AnyShaderReadUniformBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::AnyShaderReadOther:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TransferRead:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            };
        case vk_sync::AccessType::HostRead:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_HOST_BIT,
                .AccessMask = VK_ACCESS_HOST_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::Present:
            return AccessInfo{
                .StageMask = 0,
                .AccessMask = 0,
                .ImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            };
        case vk_sync::AccessType::CommandBufferWriteNVX:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                .AccessMask = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
                .ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
        case vk_sync::AccessType::VertexShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TessellationControlShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TessellationEvaluationShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::GeometryShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::FragmentShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::ColorAttachmentWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };
        case vk_sync::AccessType::DepthStencilAttachmentWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            };
        case vk_sync::AccessType::DepthAttachmentWriteStencilReadOnly:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
            };
        case vk_sync::AccessType::StencilAttachmentWriteDepthReadOnly:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
            };
        case vk_sync::AccessType::ComputeShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::AnyShaderWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::TransferWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            };
        case vk_sync::AccessType::HostWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_HOST_BIT,
                .AccessMask = VK_ACCESS_HOST_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        case vk_sync::AccessType::ColorAttachmentReadWrite:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };
        case vk_sync::AccessType::General:
            return AccessInfo{
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .AccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                .ImageLayout = VK_IMAGE_LAYOUT_GENERAL
            };
        default:
            throw std::runtime_error("Unknown access type");
    }
}

VkImageAspectFlags ImageAspectMaskFromFormat(VkFormat format) {
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

std::optional<VkImageAspectFlags> ImageAspectMaskFromAccessTypeAndFormat(
    vk_sync::AccessType accessType,
    VkFormat format
) {
    try {
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
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get image aspect mask from access type and format: " + std::string(e.what()));
    }
}

} // namespace tekki::backend::vulkan