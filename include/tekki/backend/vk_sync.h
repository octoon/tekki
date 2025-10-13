#pragma once

// This is a placeholder for vk_sync - a Vulkan synchronization helper
// The actual implementation should come from vk-sync library

#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>
#include <vector>

namespace vk_sync {

// Access types for resources
enum class AccessType {
    // Common access types
    None,
    Nothing = None,  // Alias for compatibility
    CommandBufferRead,
    CommandBufferWrite,
    CommandBufferReadNVX,
    CommandBufferWriteNVX,

    // Compute shader access
    ComputeShaderRead,
    ComputeShaderWrite,
    ComputeShaderReadUniformBuffer,
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    ComputeShaderReadOther,

    // Vertex shader access
    VertexShaderRead,
    VertexShaderWrite,
    VertexShaderReadUniformBuffer,
    VertexShaderReadSampledImageOrUniformTexelBuffer,
    VertexShaderReadOther,

    // Tessellation control shader
    TessellationControlShaderWrite,
    TessellationControlShaderReadUniformBuffer,
    TessellationControlShaderReadSampledImageOrUniformTexelBuffer,
    TessellationControlShaderReadOther,

    // Tessellation evaluation shader
    TessellationEvaluationShaderWrite,
    TessellationEvaluationShaderReadUniformBuffer,
    TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer,
    TessellationEvaluationShaderReadOther,

    // Geometry shader
    GeometryShaderWrite,
    GeometryShaderReadUniformBuffer,
    GeometryShaderReadSampledImageOrUniformTexelBuffer,
    GeometryShaderReadOther,

    // Fragment shader access
    FragmentShaderRead,
    FragmentShaderWrite,
    FragmentShaderReadUniformBuffer,
    FragmentShaderReadSampledImageOrUniformTexelBuffer,
    FragmentShaderReadColorInputAttachment,
    FragmentShaderReadDepthStencilInputAttachment,
    FragmentShaderReadOther,

    // Graphics access
    ColorAttachmentRead,
    ColorAttachmentWrite,
    ColorAttachmentReadWrite,
    DepthStencilAttachmentRead,
    DepthStencilAttachmentWrite,
    DepthAttachmentWriteStencilReadOnly,
    StencilAttachmentWriteDepthReadOnly,

    // Generic shader access
    AnyShaderWrite,
    AnyShaderReadUniformBuffer,
    AnyShaderReadUniformBufferOrVertexBuffer,
    AnyShaderReadSampledImageOrUniformTexelBuffer,
    AnyShaderReadOther,

    // Transfer operations
    TransferRead,
    TransferWrite,

    // Host access
    HostRead,
    HostWrite,

    // Present
    Present,

    // Vertex and index buffers
    IndirectBuffer,
    IndexBuffer,
    VertexBuffer,

    // General
    General,

    // Ray tracing
    RayTracingShaderRead,
    AccelerationStructureBuildRead,
    AccelerationStructureBuildWrite,
};

// Access info structure
struct AccessInfo {
    VkPipelineStageFlags stage_mask;
    VkAccessFlags access_mask;
    VkImageLayout image_layout;
};

// Image barrier structure
struct ImageBarrier {
    VkImage image;
    AccessType prev_access;
    AccessType next_access;
    VkImageAspectFlags aspect_mask;
};

// Buffer barrier structure
struct BufferBarrier {
    AccessType prev_access;
    AccessType next_access;
    uint32_t src_queue_family_index;
    uint32_t dst_queue_family_index;
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
};

// Global barrier structure
struct GlobalBarrier {
    std::vector<AccessType> previous_accesses;
    std::vector<AccessType> next_accesses;

    GlobalBarrier(const std::vector<AccessType>& prev, const std::vector<AccessType>& next)
        : previous_accesses(prev), next_accesses(next) {}
};

// Image layout for the access type
VkImageLayout GetImageLayout(AccessType access);

// Pipeline stage flags for the access type
VkPipelineStageFlags GetPipelineStageFlags(AccessType access);

// Access flags for the access type
VkAccessFlags GetAccessFlags(AccessType access);

// Get access info for a given access type
AccessInfo get_access_info(AccessType access);

// Get image aspect mask from format
VkImageAspectFlags image_aspect_mask_from_format(VkFormat format);

// Get image aspect mask from access type and format
std::optional<VkImageAspectFlags> image_aspect_mask_from_access_type_and_format(
    AccessType access_type, VkFormat format);

// Record image barrier
void record_image_barrier(
    VkDevice device,
    VkCommandBuffer cmd,
    const ImageBarrier& barrier);

// Command namespace for pipeline barriers
namespace cmd {
    void pipeline_barrier(
        VkDevice device,
        VkCommandBuffer cmd,
        std::optional<GlobalBarrier> global_barrier,
        const std::vector<BufferBarrier>& buffer_barriers,
        const std::vector<ImageBarrier>& image_barriers);
}

} // namespace vk_sync
