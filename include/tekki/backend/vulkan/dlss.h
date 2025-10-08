// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya/src/renderers/dlss.rs

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vulkan/vulkan.hpp>

#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

class Device;
class Image;

// Forward declarations for NVIDIA DLSS types
typedef void* NVSDK_NGX_Handle;
typedef void* NVSDK_NGX_Parameter;
typedef int NVSDK_NGX_Result;
typedef int NVSDK_NGX_PerfQuality_Value;
typedef int NVSDK_NGX_Feature_Create_Flags;

typedef struct NVSDK_NGX_Resource_VK
{
    struct
    {
        struct
        {
            void* ImageView;
            void* Image;
            void* SubresourceRange;
            int Format;
            unsigned int Width;
            unsigned int Height;
        } ImageViewInfo;
    } Resource;
    int Type;
    bool ReadWrite;
} NVSDK_NGX_Resource_VK;

typedef struct NVSDK_NGX_Dimensions
{
    unsigned int Width;
    unsigned int Height;
} NVSDK_NGX_Dimensions;

typedef struct NVSDK_NGX_Coordinates
{
    unsigned int X;
    unsigned int Y;
} NVSDK_NGX_Coordinates;

typedef struct NVSDK_NGX_FeatureCommonInfo
{
    struct
    {
        wchar_t** Path;
        unsigned int Length;
    } PathListInfo;
    void* InternalData;
    struct
    {
        void* LoggingCallback;
        int MinimumLoggingLevel;
        bool DisableOtherLoggingSinks;
    } LoggingInfo;
} NVSDK_NGX_FeatureCommonInfo;

typedef struct NVSDK_NGX_DLSS_Create_Params
{
    struct
    {
        unsigned int InWidth;
        unsigned int InHeight;
        unsigned int InTargetWidth;
        unsigned int InTargetHeight;
        NVSDK_NGX_PerfQuality_Value InPerfQualityValue;
    } Feature;
    NVSDK_NGX_Feature_Create_Flags InFeatureCreateFlags;
    bool InEnableOutputSubrects;
} NVSDK_NGX_DLSS_Create_Params;

typedef struct NVSDK_NGX_VK_DLSS_Eval_Params
{
    struct
    {
        NVSDK_NGX_Resource_VK* pInColor;
        NVSDK_NGX_Resource_VK* pInOutput;
        float InSharpness;
    } Feature;
    NVSDK_NGX_Resource_VK* pInDepth;
    NVSDK_NGX_Resource_VK* pInMotionVectors;
    float InJitterOffsetX;
    float InJitterOffsetY;
    NVSDK_NGX_Dimensions InRenderSubrectDimensions;
    int InReset;
    float InMVScaleX;
    float InMVScaleY;
    NVSDK_NGX_Resource_VK* pInTransparencyMask;
    NVSDK_NGX_Resource_VK* pInExposureTexture;
    NVSDK_NGX_Resource_VK* pInBiasCurrentColorMask;
    NVSDK_NGX_Coordinates InColorSubrectBase;
    NVSDK_NGX_Coordinates InDepthSubrectBase;
    NVSDK_NGX_Coordinates InMVSubrectBase;
    NVSDK_NGX_Coordinates InTranslucencySubrectBase;
    NVSDK_NGX_Coordinates InBiasCurrentColorSubrectBase;
    NVSDK_NGX_Coordinates InOutputSubrectBase;
    float InPreExposure;
    int InIndicatorInvertXAxis;
    int InIndicatorInvertYAxis;
    struct
    {
        NVSDK_NGX_Resource_VK* pInAttrib[16];
    } GBufferSurface;
    unsigned int InToneMapperType;
    NVSDK_NGX_Resource_VK* pInMotionVectors3D;
    NVSDK_NGX_Resource_VK* pInIsParticleMask;
    NVSDK_NGX_Resource_VK* pInAnimatedTextureMask;
    NVSDK_NGX_Resource_VK* pInDepthHighRes;
    NVSDK_NGX_Resource_VK* pInPositionViewSpace;
    float InFrameTimeDeltaInMsec;
    NVSDK_NGX_Resource_VK* pInRayTracingHitDistance;
    NVSDK_NGX_Resource_VK* pInMotionVectorsReflections;
} NVSDK_NGX_VK_DLSS_Eval_Params;

// DLSS optimal settings
struct DlssOptimalSettings
{
    glm::uvec2 optimal_render_extent;
    glm::uvec2 max_render_extent;
    glm::uvec2 min_render_extent;

    bool supports_input_resolution(const glm::uvec2& input_resolution) const;
};

// Main DLSS renderer
class DlssRenderer
{
public:
    DlssRenderer(std::shared_ptr<Device> device, const glm::uvec2& input_resolution,
                 const glm::uvec2& target_resolution);
    ~DlssRenderer();

    // Render with DLSS
    std::shared_ptr<Image> render(vk::CommandBuffer command_buffer, std::shared_ptr<Image> input,
                                  std::shared_ptr<Image> reprojection_map, std::shared_ptr<Image> depth,
                                  const glm::uvec2& output_extent);

    glm::vec2 current_supersample_offset() const { return current_supersample_offset_; }

private:
    void initialize_dlss(const glm::uvec2& input_resolution, const glm::uvec2& target_resolution);
    DlssOptimalSettings get_optimal_settings_for_target_resolution(NVSDK_NGX_Parameter* ngx_params,
                                                                   const glm::uvec2& target_resolution,
                                                                   NVSDK_NGX_PerfQuality_Value quality_value);
    NVSDK_NGX_Resource_VK image_to_ngx_resource(std::shared_ptr<Image> image, bool is_writable = false);

    std::shared_ptr<Device> device_;
    NVSDK_NGX_Handle* dlss_feature_{nullptr};
    NVSDK_NGX_Parameter* ngx_params_{nullptr};
    glm::vec2 current_supersample_offset_{0.0f, 0.0f};
    uint32_t frame_idx_{0};
    bool initialized_{false};
};

// DLSS utility functions
class DlssUtils
{
public:
    static bool check_dlss_availability(std::shared_ptr<Device> device);
    static std::vector<const char*> get_required_instance_extensions();
    static std::vector<const char*> get_required_device_extensions();

private:
    static NVSDK_NGX_Result nv_sdk_ngx_vulkan_init(uint32_t application_id, const wchar_t* application_data_path,
                                                   void* instance, void* physical_device, void* device,
                                                   const NVSDK_NGX_FeatureCommonInfo* feature_common_info,
                                                   uint32_t version);

    static NVSDK_NGX_Result nv_sdk_ngx_vulkan_get_capability_parameters(NVSDK_NGX_Parameter** parameters);
    static NVSDK_NGX_Result nv_sdk_ngx_vulkan_create_feature(void* command_list, int feature_id,
                                                             NVSDK_NGX_Parameter* parameters,
                                                             NVSDK_NGX_Handle** feature_handle);
    static NVSDK_NGX_Result nv_sdk_ngx_vulkan_evaluate_feature(void* command_list, NVSDK_NGX_Handle* feature_handle,
                                                               NVSDK_NGX_Parameter* parameters, void* callback);
    static void nv_sdk_ngx_vulkan_release_feature(NVSDK_NGX_Handle* feature_handle);
    static void nv_sdk_ngx_vulkan_shutdown();
};

} // namespace tekki::backend::vulkan