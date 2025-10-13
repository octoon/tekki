#pragma once

// Stub header for NVIDIA NGX DLSS SDK
// This is a placeholder to allow compilation without the actual DLSS SDK
// Replace with the real NVIDIA NGX SDK headers when available

#include <cstdint>
#include <vulkan/vulkan.h>

// DLSS result codes
typedef enum NVSDK_NGX_Result {
    NVSDK_NGX_Result_NVSDK_NGX_Result_Success = 0,
    NVSDK_NGX_Result_Fail = 1
} NVSDK_NGX_Result;

// DLSS quality settings
typedef enum NVSDK_NGX_PerfQuality_Value {
    NVSDK_NGX_PerfQuality_Value_MaxPerf = 0,
    NVSDK_NGX_PerfQuality_Value_Balanced = 1,
    NVSDK_NGX_PerfQuality_Value_MaxQuality = 2,
    NVSDK_NGX_PerfQuality_Value_UltraPerformance = 3,
    NVSDK_NGX_PerfQuality_Value_UltraQuality = 4
} NVSDK_NGX_PerfQuality_Value;

// Vulkan resource wrapper
typedef struct NVSDK_NGX_Resource_VK {
    VkImage image;
    VkImageView imageView;
    VkImageSubresourceRange subresourceRange;
    VkFormat format;
    uint32_t width;
    uint32_t height;
} NVSDK_NGX_Resource_VK;

// DLSS evaluation parameters
typedef struct NVSDK_NGX_VK_DLSS_Eval_Params {
    NVSDK_NGX_Resource_VK* pInColor;
    NVSDK_NGX_Resource_VK* pInOutput;
    NVSDK_NGX_Resource_VK* pInDepth;
    NVSDK_NGX_Resource_VK* pInMotionVectors;
    float sharpness;
    float jitterOffsetX;
    float jitterOffsetY;
    int resetAccumulation;
} NVSDK_NGX_VK_DLSS_Eval_Params;
