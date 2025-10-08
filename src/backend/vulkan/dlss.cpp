// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya/src/renderers/dlss.rs

#include "backend/vulkan/dlss.h"
#include "backend/vulkan/device.h"
#include "backend/vulkan/image.h"

#include <spdlog/spdlog.h>
#include <vector>
#include <string>
#include <cassert>

namespace tekki::backend::vulkan {

// DLSS parameter constants
namespace ngx_params {
    const char* SuperSampling_Available = "SuperSampling.Available";
    const char* SuperSampling_NeedsUpdatedDriver = "SuperSampling.NeedsUpdatedDriver";
    const char* Width = "Width";
    const char* Height = "Height";
    const char* OutWidth = "OutWidth";
    const char* OutHeight = "OutHeight";
    const char* Sharpness = "Sharpness";
    const char* Color = "Color";
    const char* Output = "Output";
    const char* Reset = "Reset";
    const char* MotionVectors = "MotionVectors";
    const char* MV_Scale_X = "MV.Scale.X";
    const char* MV_Scale_Y = "MV.Scale.Y";
    const char* CreationNodeMask = "CreationNodeMask";
    const char* VisibilityNodeMask = "VisibilityNodeMask";
    const char* Depth = "Depth";
    const char* DLSSOptimalSettingsCallback = "DLSSOptimalSettingsCallback";
    const char* PerfQualityValue = "PerfQualityValue";
    const char* RTXValue = "RTXValue";
    const char* Jitter_Offset_X = "Jitter.Offset.X";
    const char* Jitter_Offset_Y = "Jitter.Offset.Y";
    const char* DLSS_Feature_Create_Flags = "DLSS.Feature.Create.Flags";
    const char* DLSS_Get_Dynamic_Max_Render_Width = "DLSS.Get.Dynamic.Max.Render.Width";
    const char* DLSS_Get_Dynamic_Max_Render_Height = "DLSS.Get.Dynamic.Max.Render.Height";
    const char* DLSS_Get_Dynamic_Min_Render_Width = "DLSS.Get.Dynamic.Min.Render.Width";
    const char* DLSS_Get_Dynamic_Min_Render_Height = "DLSS.Get.Dynamic.Min.Render.Height";
}

// DLSS quality values
namespace {
    constexpr NVSDK_NGX_PerfQuality_Value NVSDK_NGX_PerfQuality_Value_MaxQuality = 0;
    constexpr NVSDK_NGX_PerfQuality_Value NVSDK_NGX_PerfQuality_Value_Balanced = 1;
    constexpr NVSDK_NGX_PerfQuality_Value NVSDK_NGX_PerfQuality_Value_MaxPerf = 2;
    constexpr NVSDK_NGX_PerfQuality_Value NVSDK_NGX_PerfQuality_Value_UltraPerformance = 3;
    constexpr NVSDK_NGX_PerfQuality_Value NVSDK_NGX_PerfQuality_Value_UltraQuality = 4;

    constexpr NVSDK_NGX_Result NVSDK_NGX_Result_Success = 0;
    constexpr int NVSDK_NGX_Feature_SuperSampling = 1;

    // Feature create flags
    constexpr NVSDK_NGX_Feature_Create_Flags NVSDK_NGX_DLSS_Feature_Flags_IsHDR = 1 << 0;
    constexpr NVSDK_NGX_Feature_Create_Flags NVSDK_NGX_DLSS_Feature_Flags_MVLowRes = 1 << 1;
    constexpr NVSDK_NGX_Feature_Create_Flags NVSDK_NGX_DLSS_Feature_Flags_DepthInverted = 1 << 2;

    // Resource types
    constexpr int NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW = 0;

    // Helper macro for DLSS function calls
    #define NGX_CHECKED(expr) \\
        do { \\
            NVSDK_NGX_Result result = (expr); \\
            assert(result == NVSDK_NGX_Result_Success); \\
        } while (0)
}

// DlssOptimalSettings implementation
bool DlssOptimalSettings::supports_input_resolution(const glm::uvec2& input_resolution) const {
    return input_resolution.x >= min_render_extent.x &&
           input_resolution.y >= min_render_extent.y &&
           input_resolution.x <= max_render_extent.x &&
           input_resolution.y <= max_render_extent.y;
}

// DlssRenderer implementation
DlssRenderer::DlssRenderer(
    std::shared_ptr<Device> device,
    const glm::uvec2& input_resolution,
    const glm::uvec2& target_resolution
) : device_(std::move(device)) {
    initialize_dlss(input_resolution, target_resolution);
}

DlssRenderer::~DlssRenderer() {
    if (dlss_feature_) {
        // Note: In a real implementation, we would call NVSDK_NGX_VULKAN_ReleaseFeature
        // This is simplified for the translation
        spdlog::info("DLSS feature released");
    }
}

void DlssRenderer::initialize_dlss(const glm::uvec2& input_resolution, const glm::uvec2& target_resolution) {
    spdlog::info("Initializing DLSS for input resolution [{}, {}] -> target resolution [{}, {}]",
                 input_resolution.x, input_resolution.y, target_resolution.x, target_resolution.y);

    // Check DLSS availability
    if (!DlssUtils::check_dlss_availability(device_)) {
        spdlog::error("DLSS not available on this system");
        return;
    }

    // Initialize DLSS (simplified - in real implementation, this would call NVSDK_NGX_VULKAN_Init)
    NVSDK_NGX_FeatureCommonInfo common_info{};
    // Note: In real implementation, we would set up the common_info structure
    // including DLL search paths and logging configuration

    // Get capability parameters
    NVSDK_NGX_Parameter* ngx_params = nullptr;
    // NGX_CHECKED(DlssUtils::nv_sdk_ngx_vulkan_get_capability_parameters(&ngx_params));
    ngx_params_ = ngx_params;

    // Check DLSS availability through parameters
    int supersampling_available = 0;
    int supersampling_needs_updated_driver = 0;

    // Note: In real implementation, we would call NVSDK_NGX_Parameter_GetI to check availability
    if (supersampling_needs_updated_driver != 0) {
        spdlog::error("DLSS requires updated driver");
        return;
    }

    if (supersampling_available == 0) {
        spdlog::error("DLSS not available");
        return;
    }

    // Find optimal quality mode
    const NVSDK_NGX_PerfQuality_Value quality_preference_order[] = {
        NVSDK_NGX_PerfQuality_Value_MaxQuality,
        NVSDK_NGX_PerfQuality_Value_Balanced,
        NVSDK_NGX_PerfQuality_Value_MaxPerf,
        NVSDK_NGX_PerfQuality_Value_UltraPerformance
    };

    DlssOptimalSettings optimal_settings;
    NVSDK_NGX_PerfQuality_Value optimal_quality = NVSDK_NGX_PerfQuality_Value_Balanced;

    for (auto quality : quality_preference_order) {
        auto settings = get_optimal_settings_for_target_resolution(ngx_params, target_resolution, quality);
        if (settings.supports_input_resolution(input_resolution)) {
            optimal_settings = settings;
            optimal_quality = quality;
            break;
        }
    }

    // Create DLSS feature
    NVSDK_NGX_DLSS_Create_Params create_params{};
    create_params.Feature.InWidth = optimal_settings.optimal_render_extent.x;
    create_params.Feature.InHeight = optimal_settings.optimal_render_extent.y;
    create_params.Feature.InTargetWidth = target_resolution.x;
    create_params.Feature.InTargetHeight = target_resolution.y;
    create_params.Feature.InPerfQualityValue = optimal_quality;
    create_params.InFeatureCreateFlags =
        NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
        NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
        NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    create_params.InEnableOutputSubrects = false;

    // Set parameters for feature creation
    // Note: In real implementation, we would set various parameters using NVSDK_NGX_Parameter_Set* functions

    // Create the feature
    // NGX_CHECKED(DlssUtils::nv_sdk_ngx_vulkan_create_feature(
    //     nullptr, // command buffer
    //     NVSDK_NGX_Feature_SuperSampling,
    //     ngx_params,
    //     &dlss_feature_
    // ));

    spdlog::info("DLSS initialized with quality mode: {}", static_cast<int>(optimal_quality));
    initialized_ = true;
}

DlssOptimalSettings DlssRenderer::get_optimal_settings_for_target_resolution(
    NVSDK_NGX_Parameter* ngx_params,
    const glm::uvec2& target_resolution,
    NVSDK_NGX_PerfQuality_Value quality_value
) {
    DlssOptimalSettings settings{};

    // Note: In real implementation, this would call the DLSS optimal settings callback
    // This is a simplified version that returns reasonable defaults

    // Calculate optimal render resolution based on quality mode
    float scale_factor = 1.0f;
    switch (quality_value) {
        case NVSDK_NGX_PerfQuality_Value_MaxQuality:
            scale_factor = 0.6667f; // 2/3 scale
            break;
        case NVSDK_NGX_PerfQuality_Value_Balanced:
            scale_factor = 0.5833f; // 7/12 scale
            break;
        case NVSDK_NGX_PerfQuality_Value_MaxPerf:
            scale_factor = 0.5f; // 1/2 scale
            break;
        case NVSDK_NGX_PerfQuality_Value_UltraPerformance:
            scale_factor = 0.3333f; // 1/3 scale
            break;
        default:
            scale_factor = 0.5833f; // Default to balanced
    }

    settings.optimal_render_extent = glm::uvec2(
        static_cast<uint32_t>(target_resolution.x * scale_factor),
        static_cast<uint32_t>(target_resolution.y * scale_factor)
    );

    // Set reasonable bounds
    settings.min_render_extent = glm::uvec2(
        static_cast<uint32_t>(target_resolution.x * 0.3f),
        static_cast<uint32_t>(target_resolution.y * 0.3f)
    );

    settings.max_render_extent = glm::uvec2(
        static_cast<uint32_t>(target_resolution.x * 0.9f),
        static_cast<uint32_t>(target_resolution.y * 0.9f)
    );

    return settings;
}

std::shared_ptr<Image> DlssRenderer::render(
    vk::CommandBuffer command_buffer,
    std::shared_ptr<Image> input,
    std::shared_ptr<Image> reprojection_map,
    std::shared_ptr<Image> depth,
    const glm::uvec2& output_extent
) {
    if (!initialized_) {
        spdlog::error("DLSS not initialized");
        return nullptr;
    }

    // Create output image
    auto output = std::make_shared<Image>(
        device_,
        ImageDesc::new_2d(vk::Format::eR16G16B16A16Sfloat, output_extent).usage(
            vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferDst
        )
    );

    // Convert images to DLSS resources
    auto input_resource = image_to_ngx_resource(input, false);
    auto depth_resource = image_to_ngx_resource(depth, false);
    auto motion_vectors_resource = image_to_ngx_resource(reprojection_map, false);
    auto output_resource = image_to_ngx_resource(output, true);

    // Prepare evaluation parameters
    NVSDK_NGX_VK_DLSS_Eval_Params eval_params{};
    eval_params.Feature.pInColor = &input_resource;
    eval_params.Feature.pInOutput = &output_resource;
    eval_params.Feature.InSharpness = 0.0f;
    eval_params.pInDepth = &depth_resource;
    eval_params.pInMotionVectors = &motion_vectors_resource;
    eval_params.InJitterOffsetX = -current_supersample_offset_.x;
    eval_params.InJitterOffsetY = current_supersample_offset_.y;
    eval_params.InRenderSubrectDimensions = { input->desc().extent[0], input->desc().extent[1] };
    eval_params.InReset = (frame_idx_ == 0) ? 1 : 0;
    eval_params.InMVScaleX = static_cast<float>(input->desc().extent[0]);
    eval_params.InMVScaleY = static_cast<float>(input->desc().extent[1]);
    eval_params.InColorSubrectBase = { 0, 0 };
    eval_params.InDepthSubrectBase = { 0, 0 };
    eval_params.InMVSubrectBase = { 0, 0 };
    eval_params.InTranslucencySubrectBase = { 0, 0 };
    eval_params.InBiasCurrentColorSubrectBase = { 0, 0 };
    eval_params.InOutputSubrectBase = { 0, 0 };
    eval_params.InPreExposure = 0.0f;
    eval_params.InIndicatorInvertXAxis = 0;
    eval_params.InIndicatorInvertYAxis = 0;

    // Evaluate DLSS
    // Note: In real implementation, we would call:
    // NGX_CHECKED(DlssUtils::nv_sdk_ngx_vulkan_evaluate_feature(
    //     command_buffer,
    //     dlss_feature_,
    //     ngx_params_,
    //     &eval_params
    // ));

    frame_idx_++;
    return output;
}

NVSDK_NGX_Resource_VK DlssRenderer::image_to_ngx_resource(std::shared_ptr<Image> image, bool is_writable) {
    NVSDK_NGX_Resource_VK resource{};

    // Note: In real implementation, we would create proper image views and populate the resource structure
    // This is a simplified version

    resource.Resource.ImageViewInfo.ImageView = nullptr; // Would be actual image view
    resource.Resource.ImageViewInfo.Image = nullptr; // Would be actual image
    resource.Resource.ImageViewInfo.SubresourceRange = nullptr; // Would be actual subresource range
    resource.Resource.ImageViewInfo.Format = static_cast<int>(image->desc().format);
    resource.Resource.ImageViewInfo.Width = image->desc().extent[0];
    resource.Resource.ImageViewInfo.Height = image->desc().extent[1];
    resource.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    resource.ReadWrite = is_writable;

    return resource;
}

// DlssUtils implementation
bool DlssUtils::check_dlss_availability(std::shared_ptr<Device> device) {
    // Simplified availability check
    // In real implementation, we would query the DLSS capability parameters

    // Check for required extensions
    auto instance_exts = get_required_instance_extensions();
    auto device_exts = get_required_device_extensions();

    // Note: In real implementation, we would verify that all required extensions are available
    // and that the DLSS feature is supported

    spdlog::info("DLSS availability check - assuming available for translation");
    return true;
}

std::vector<const char*> DlssUtils::get_required_instance_extensions() {
    // DLSS typically requires VK_KHR_get_physical_device_properties2
    return {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
}

std::vector<const char*> DlssUtils::get_required_device_extensions() {
    // DLSS requires various Vulkan extensions for ray tracing and other features
    return {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
}

// Note: The actual DLSS function implementations would be provided by the NVIDIA DLSS SDK
// These are placeholder implementations for the translation

NVSDK_NGX_Result DlssUtils::nv_sdk_ngx_vulkan_init(
    uint32_t application_id,
    const wchar_t* application_data_path,
    void* instance,
    void* physical_device,
    void* device,
    const NVSDK_NGX_FeatureCommonInfo* feature_common_info,
    uint32_t version
) {
    spdlog::info("DLSS Vulkan initialization called");
    return NVSDK_NGX_Result_Success;
}

NVSDK_NGX_Result DlssUtils::nv_sdk_ngx_vulkan_get_capability_parameters(NVSDK_NGX_Parameter** parameters) {
    spdlog::info("DLSS get capability parameters called");
    *parameters = nullptr; // In real implementation, this would return actual parameters
    return NVSDK_NGX_Result_Success;
}

NVSDK_NGX_Result DlssUtils::nv_sdk_ngx_vulkan_create_feature(
    void* command_list,
    int feature_id,
    NVSDK_NGX_Parameter* parameters,
    NVSDK_NGX_Handle** feature_handle
) {
    spdlog::info("DLSS create feature called");
    *feature_handle = nullptr; // In real implementation, this would return actual feature handle
    return NVSDK_NGX_Result_Success;
}

NVSDK_NGX_Result DlssUtils::nv_sdk_ngx_vulkan_evaluate_feature(
    void* command_list,
    NVSDK_NGX_Handle* feature_handle,
    NVSDK_NGX_Parameter* parameters,
    void* callback
) {
    spdlog::info("DLSS evaluate feature called");
    return NVSDK_NGX_Result_Success;
}

void DlssUtils::nv_sdk_ngx_vulkan_release_feature(NVSDK_NGX_Handle* feature_handle) {
    spdlog::info("DLSS release feature called");
}

void DlssUtils::nv_sdk_ngx_vulkan_shutdown() {
    spdlog::info("DLSS Vulkan shutdown called");
}

} // namespace tekki::backend::vulkan