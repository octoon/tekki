#pragma once

// This is a placeholder for vk_sync - a Vulkan synchronization helper
// The actual implementation should come from vk-sync library

#include <vulkan/vulkan.h>
#include <cstdint>

namespace vk_sync {

// Access types for resources
enum class AccessType {
    // Common access types
    None,
    CommandBufferRead,
    CommandBufferWrite,

    // Compute shader access
    ComputeShaderRead,
    ComputeShaderWrite,
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    ComputeShaderReadOther,

    // Graphics access
    VertexShaderRead,
    FragmentShaderRead,
    FragmentShaderWrite,
    ColorAttachmentRead,
    ColorAttachmentWrite,
    DepthStencilAttachmentRead,
    DepthStencilAttachmentWrite,

    // Transfer operations
    TransferRead,
    TransferWrite,

    // Host access
    HostRead,
    HostWrite,

    // Present
    Present,

    // Ray tracing
    RayTracingShaderRead,
    AccelerationStructureBuildRead,
    AccelerationStructureBuildWrite,
};

// Image layout for the access type
VkImageLayout GetImageLayout(AccessType access);

// Pipeline stage flags for the access type
VkPipelineStageFlags GetPipelineStageFlags(AccessType access);

// Access flags for the access type
VkAccessFlags GetAccessFlags(AccessType access);

} // namespace vk_sync
