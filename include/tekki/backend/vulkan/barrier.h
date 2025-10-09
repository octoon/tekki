```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "device.h"

namespace tekki::backend::vulkan {

class ImageBarrier {
public:
    ImageBarrier(
        VkImage image,
        vk_sync::AccessType prevAccess,
        vk_sync::AccessType nextAccess,
        VkImageAspectFlags aspectMask
    ) : Image(image), PrevAccess(prevAccess), NextAccess(nextAccess), Discard(false), AspectMask(aspectMask) {}

    ImageBarrier WithDiscard(bool discard) {
        Discard = discard;
        return *this;
    }

    VkImage Image;
    vk_sync::AccessType PrevAccess;
    vk_sync::AccessType NextAccess;
    bool Discard;
    VkImageAspectFlags AspectMask;
};

inline void RecordImageBarrier(std::shared_ptr<Device> device, VkCommandBuffer commandBuffer, ImageBarrier barrier) {
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
}

struct AccessInfo {
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout ImageLayout;
};

inline AccessInfo GetAccessInfo(vk_sync::AccessType accessType) {
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
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT