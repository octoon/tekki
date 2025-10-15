#pragma once

// Stub header for NVIDIA NGX DLSS SDK
// This is a placeholder to allow compilation without the actual DLSS SDK
// Replace with the real NVIDIA NGX SDK headers when available

#include <cstdint>
#include <vulkan/vulkan.h>

// Forward declarations
typedef struct NVSDK_NGX_Parameter NVSDK_NGX_Parameter;
typedef struct NVSDK_NGX_Handle NVSDK_NGX_Handle;

// DLSS result codes
typedef enum NVSDK_NGX_Result {
    NVSDK_NGX_Result_NVSDK_NGX_Result_Success = 0,
    NVSDK_NGX_Result_NVSDK_NGX_Result_Fail = 1
} NVSDK_NGX_Result;

// DLSS quality settings
typedef enum NVSDK_NGX_PerfQuality_Value {
    NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxPerf = 0,
    NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_Balanced = 1,
    NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxQuality = 2,
    NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_UltraPerformance = 3,
    NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_UltraQuality = 4
} NVSDK_NGX_PerfQuality_Value;

// DLSS feature types
typedef enum NVSDK_NGX_Feature {
    NVSDK_NGX_Feature_NVSDK_NGX_Feature_SuperSampling = 0
} NVSDK_NGX_Feature;

// DLSS version
typedef enum NVSDK_NGX_Version {
    NVSDK_NGX_Version_NVSDK_NGX_Version_API = 1
} NVSDK_NGX_Version;

// DLSS logging levels
typedef enum NVSDK_NGX_Logging_Level {
    NVSDK_NGX_Logging_Level_NVSDK_NGX_LOGGING_LEVEL_OFF = 0,
    NVSDK_NGX_Logging_Level_NVSDK_NGX_LOGGING_LEVEL_VERBOSE = 1
} NVSDK_NGX_Logging_Level;

// DLSS feature flags
typedef enum NVSDK_NGX_DLSS_Feature_Flags {
    NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_IsHDR = 1 << 0,
    NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_MVLowRes = 1 << 1,
    NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_DepthInverted = 1 << 2
} NVSDK_NGX_DLSS_Feature_Flags;

// Tonemapper types
typedef enum NVSDK_NGX_ToneMapperType {
    NVSDK_NGX_ToneMapperType_NVSDK_NGX_TONEMAPPER_STRING = 0
} NVSDK_NGX_ToneMapperType;

// Structures
struct NVSDK_NGX_Dimensions {
    uint32_t Width;
    uint32_t Height;
};

struct NVSDK_NGX_Coordinates {
    uint32_t X;
    uint32_t Y;
};

struct NVSDK_NGX_Feature_Create_Params {
    uint32_t InWidth;
    uint32_t InHeight;
    uint32_t InTargetWidth;
    uint32_t InTargetHeight;
    NVSDK_NGX_PerfQuality_Value InPerfQualityValue;
};

struct NVSDK_NGX_VK_Feature_Eval_Params {
    void* pInColor;
    void* pInOutput;
    float InSharpness;
};

struct NVSDK_NGX_VK_GBuffer {
    void* pInAttrib[16];
};

struct NVSDK_NGX_DLSS_Create_Params {
    NVSDK_NGX_Feature_Create_Params Feature;
    int32_t InFeatureCreateFlags;
    bool InEnableOutputSubrects;
};

struct NVSDK_NGX_VK_DLSS_Eval_Params {
    NVSDK_NGX_VK_Feature_Eval_Params Feature;
    void* pInDepth;
    void* pInMotionVectors;
    float InJitterOffsetX;
    float InJitterOffsetY;
    NVSDK_NGX_Dimensions InRenderSubrectDimensions;
    int32_t InReset;
    float InMVScaleX;
    float InMVScaleY;
    void* pInTransparencyMask;
    void* pInExposureTexture;
    void* pInBiasCurrentColorMask;
    NVSDK_NGX_Coordinates InColorSubrectBase;
    NVSDK_NGX_Coordinates InDepthSubrectBase;
    NVSDK_NGX_Coordinates InMVSubrectBase;
    NVSDK_NGX_Coordinates InTranslucencySubrectBase;
    NVSDK_NGX_Coordinates InBiasCurrentColorSubrectBase;
    NVSDK_NGX_Coordinates InOutputSubrectBase;
    float InPreExposure;
    int32_t InIndicatorInvertXAxis;
    int32_t InIndicatorInvertYAxis;
    NVSDK_NGX_VK_GBuffer GBufferSurface;
    NVSDK_NGX_ToneMapperType InToneMapperType;
    void* pInMotionVectors3D;
    void* pInIsParticleMask;
    void* pInAnimatedTextureMask;
    void* pInDepthHighRes;
    void* pInPositionViewSpace;
    float InFrameTimeDeltaInMsec;
    void* pInRayTracingHitDistance;
    void* pInMotionVectorsReflections;
};

struct NVSDK_NGX_PathListInfo {
    wchar_t** Path;
    uint32_t Length;
};

struct NGSDK_NGX_LoggingInfo {
    void* LoggingCallback;
    NVSDK_NGX_Logging_Level MinimumLoggingLevel;
    bool DisableOtherLoggingSinks;
};

struct NVSDK_NGX_FeatureCommonInfo {
    NVSDK_NGX_PathListInfo PathListInfo;
    void* InternalData;
    NGSDK_NGX_LoggingInfo LoggingInfo;
};

struct NVSDK_NGX_ImageViewInfo_VK {
    VkImageView ImageView;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
    VkFormat Format;
    uint32_t Width;
    uint32_t Height;
};

struct NVSDK_NGX_Resource_VK__bindgen_ty_1 {
    NVSDK_NGX_ImageViewInfo_VK ImageViewInfo;
};

typedef enum NVSDK_NGX_Resource_VK_Type {
    NVSDK_NGX_Resource_VK_Type_NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW = 0
} NVSDK_NGX_Resource_VK_Type;

// Vulkan resource wrapper
typedef struct NVSDK_NGX_Resource_VK {
    NVSDK_NGX_Resource_VK__bindgen_ty_1 Resource;
    NVSDK_NGX_Resource_VK_Type Type;
    bool ReadWrite;
} NVSDK_NGX_Resource_VK;

// Function pointer types
typedef NVSDK_NGX_Result (*PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback)(NVSDK_NGX_Parameter*);

// Function declarations (stubs)
extern "C" {
    NVSDK_NGX_Result NVSDK_NGX_VULKAN_RequiredExtensions(
        uint32_t* instExtCount,
        const char*** instExts,
        uint32_t* deviceExtCount,
        const char*** deviceExts
    );

    NVSDK_NGX_Result NVSDK_NGX_VULKAN_Init(
        uint32_t appId,
        const wchar_t* logDir,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        const NVSDK_NGX_FeatureCommonInfo* commonInfo,
        NVSDK_NGX_Version version
    );

    NVSDK_NGX_Result NVSDK_NGX_VULKAN_GetCapabilityParameters(NVSDK_NGX_Parameter** params);

    NVSDK_NGX_Result NVSDK_NGX_VULKAN_CreateFeature(
        VkCommandBuffer commandBuffer,
        NVSDK_NGX_Feature feature,
        NVSDK_NGX_Parameter* params,
        NVSDK_NGX_Handle** handle
    );

    NVSDK_NGX_Result NVSDK_NGX_VULKAN_EvaluateFeature_C(
        VkCommandBuffer commandBuffer,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* params,
        void* reserved
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_SetVoidPointer(
        NVSDK_NGX_Parameter* params,
        const char* name,
        void* value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_SetF(
        NVSDK_NGX_Parameter* params,
        const char* name,
        float value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_SetI(
        NVSDK_NGX_Parameter* params,
        const char* name,
        int32_t value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_SetUI(
        NVSDK_NGX_Parameter* params,
        const char* name,
        uint32_t value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_GetVoidPointer(
        NVSDK_NGX_Parameter* params,
        const char* name,
        void** value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_GetI(
        NVSDK_NGX_Parameter* params,
        const char* name,
        int32_t* value
    );

    NVSDK_NGX_Result NVSDK_NGX_Parameter_GetUI(
        NVSDK_NGX_Parameter* params,
        const char* name,
        uint32_t* value
    );
}
