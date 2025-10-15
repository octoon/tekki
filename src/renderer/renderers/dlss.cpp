#include "tekki/renderer/renderers/dlss.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "ngx_dlss.h"
#include "tekki/backend/render_backend.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/RenderGraph.h"

namespace tekki::renderer::renderers {

namespace {

const char* NVSDK_NGX_Parameter_SuperSampling_Available = "SuperSampling.Available";
const char* NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver = "SuperSampling.NeedsUpdatedDriver";
const char* NVSDK_NGX_Parameter_Width = "Width";
const char* NVSDK_NGX_Parameter_Height = "Height";
const char* NVSDK_NGX_Parameter_OutWidth = "OutWidth";
const char* NVSDK_NGX_Parameter_OutHeight = "OutHeight";
const char* NVSDK_NGX_Parameter_Sharpness = "Sharpness";
const char* NVSDK_NGX_Parameter_Color = "Color";
const char* NVSDK_NGX_Parameter_Output = "Output";
const char* NVSDK_NGX_Parameter_Reset = "Reset";
const char* NVSDK_NGX_Parameter_MotionVectors = "MotionVectors";
const char* NVSDK_NGX_Parameter_MV_Scale_X = "MV.Scale.X";
const char* NVSDK_NGX_Parameter_MV_Scale_Y = "MV.Scale.Y";
const char* NVSDK_NGX_Parameter_CreationNodeMask = "CreationNodeMask";
const char* NVSDK_NGX_Parameter_VisibilityNodeMask = "VisibilityNodeMask";
const char* NVSDK_NGX_Parameter_Depth = "Depth";
const char* NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback = "DLSSOptimalSettingsCallback";
const char* NVSDK_NGX_Parameter_PerfQualityValue = "PerfQualityValue";
const char* NVSDK_NGX_Parameter_RTXValue = "RTXValue";
const char* NVSDK_NGX_Parameter_Jitter_Offset_X = "Jitter.Offset.X";
const char* NVSDK_NGX_Parameter_Jitter_Offset_Y = "Jitter.Offset.Y";
const char* NVSDK_NGX_Parameter_TransparencyMask = "TransparencyMask";
const char* NVSDK_NGX_Parameter_ExposureTexture = "ExposureTexture";
const char* NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags = "DLSS.Feature.Create.Flags";
const char* NVSDK_NGX_Parameter_TonemapperType = "TonemapperType";
const char* NVSDK_NGX_Parameter_MotionVectors3D = "MotionVectors3D";
const char* NVSDK_NGX_Parameter_IsParticleMask = "IsParticleMask";
const char* NVSDK_NGX_Parameter_AnimatedTextureMask = "AnimatedTextureMask";
const char* NVSDK_NGX_Parameter_DepthHighRes = "DepthHighRes";
const char* NVSDK_NGX_Parameter_Position_ViewSpace = "Position.ViewSpace";
const char* NVSDK_NGX_Parameter_FrameTimeDeltaInMsec = "FrameTimeDeltaInMsec";
const char* NVSDK_NGX_Parameter_RayTracingHitDistance = "RayTracingHitDistance";
const char* NVSDK_NGX_Parameter_MotionVectorsReflection = "MotionVectorsReflection";
const char* NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects = "DLSS.Enable.Output.Subrects";
const char* NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X = "DLSS.Input.Color.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y = "DLSS.Input.Color.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X = "DLSS.Input.Depth.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y = "DLSS.Input.Depth.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X = "DLSS.Input.MV.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y = "DLSS.Input.MV.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X = "DLSS.Input.Translucency.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y = "DLSS.Input.Translucency.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X = "DLSS.Output.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y = "DLSS.Output.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width = "DLSS.Render.Subrect.Dimensions.Width";
const char* NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height = "DLSS.Render.Subrect.Dimensions.Height";
const char* NVSDK_NGX_Parameter_DLSS_Pre_Exposure = "DLSS.Pre.Exposure";
const char* NVSDK_NGX_Parameter_DLSS_Exposure_Scale = "DLSS.Exposure.Scale";
const char* NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask = "DLSS.Input.Bias.Current.Color.Mask";
const char* NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X = "DLSS.Input.Bias.Current.Color.Subrect.Base.X";
const char* NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y = "DLSS.Input.Bias.Current.Color.Subrect.Base.Y";
const char* NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis = "DLSS.Indicator.Invert.Y.Axis";
const char* NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis = "DLSS.Indicator.Invert.X.Axis";
const char* NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width = "DLSS.Get.Dynamic.Max.Render.Width";
const char* NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height = "DLSS.Get.Dynamic.Max.Render.Height";
const char* NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width = "DLSS.Get.Dynamic.Min.Render.Width";
const char* NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height = "DLSS.Get.Dynamic.Min.Render.Height";

typedef NVSDK_NGX_Result (*PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback)(NVSDK_NGX_Parameter*);

NVSDK_NGX_Result NgxVulkanEvaluateDlssExt(VkCommandBuffer commandBuffer,
                                         NVSDK_NGX_Handle* handle,
                                         NVSDK_NGX_Parameter* parameters,
                                         const NVSDK_NGX_VK_DLSS_Eval_Params& dlssEvalParams) {
    try {
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_Color,
            dlssEvalParams.Feature.pInColor
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_Output,
            dlssEvalParams.Feature.pInOutput
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_Depth,
            dlssEvalParams.pInDepth
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_MotionVectors,
            dlssEvalParams.pInMotionVectors
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_Jitter_Offset_X,
            dlssEvalParams.InJitterOffsetX
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_Jitter_Offset_Y,
            dlssEvalParams.InJitterOffsetY
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_Sharpness,
            dlssEvalParams.Feature.InSharpness
        );
        NVSDK_NGX_Parameter_SetI(
            parameters,
            NVSDK_NGX_Parameter_Reset,
            dlssEvalParams.InReset
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_MV_Scale_X,
            (dlssEvalParams.InMVScaleX == 0.0f) ? 1.0f : dlssEvalParams.InMVScaleX
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_MV_Scale_Y,
            (dlssEvalParams.InMVScaleY == 0.0f) ? 1.0f : dlssEvalParams.InMVScaleY
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_TransparencyMask,
            dlssEvalParams.pInTransparencyMask
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_ExposureTexture,
            dlssEvalParams.pInExposureTexture
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask,
            dlssEvalParams.pInBiasCurrentColorMask
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_TonemapperType,
            dlssEvalParams.InToneMapperType
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_MotionVectors3D,
            dlssEvalParams.pInMotionVectors3D
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_IsParticleMask,
            dlssEvalParams.pInIsParticleMask
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_AnimatedTextureMask,
            dlssEvalParams.pInAnimatedTextureMask
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_DepthHighRes,
            dlssEvalParams.pInDepthHighRes
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_Position_ViewSpace,
            dlssEvalParams.pInPositionViewSpace
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_FrameTimeDeltaInMsec,
            dlssEvalParams.InFrameTimeDeltaInMsec
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_RayTracingHitDistance,
            dlssEvalParams.pInRayTracingHitDistance
        );
        NVSDK_NGX_Parameter_SetVoidPointer(
            parameters,
            NVSDK_NGX_Parameter_MotionVectorsReflection,
            dlssEvalParams.pInMotionVectorsReflections
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X,
            dlssEvalParams.InColorSubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y,
            dlssEvalParams.InColorSubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X,
            dlssEvalParams.InDepthSubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y,
            dlssEvalParams.InDepthSubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X,
            dlssEvalParams.InMVSubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y,
            dlssEvalParams.InMVSubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X,
            dlssEvalParams.InTranslucencySubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y,
            dlssEvalParams.InTranslucencySubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X,
            dlssEvalParams.InBiasCurrentColorSubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y,
            dlssEvalParams.InBiasCurrentColorSubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X,
            dlssEvalParams.InOutputSubrectBase.X
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y,
            dlssEvalParams.InOutputSubrectBase.Y
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,
            dlssEvalParams.InRenderSubrectDimensions.Width
        );
        NVSDK_NGX_Parameter_SetUI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,
            dlssEvalParams.InRenderSubrectDimensions.Height
        );
        NVSDK_NGX_Parameter_SetF(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Pre_Exposure,
            (dlssEvalParams.InPreExposure == 0.0f) ? 1.0f : dlssEvalParams.InPreExposure
        );
        NVSDK_NGX_Parameter_SetI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis,
            dlssEvalParams.InIndicatorInvertXAxis
        );
        NVSDK_NGX_Parameter_SetI(
            parameters,
            NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis,
            dlssEvalParams.InIndicatorInvertYAxis
        );

        return NVSDK_NGX_VULKAN_EvaluateFeature_C(commandBuffer, handle, parameters, nullptr);
    } catch (const std::exception&) {
        return NVSDK_NGX_Result_NVSDK_NGX_Result_Fail;
    }
}

NVSDK_NGX_Resource_VK ImageToNgx(tekki::render_graph::RenderPassApi& api,
                                tekki::render_graph::Ref<tekki::backend::vulkan::Image, tekki::render_graph::GpuSrv> imageRef,
                                tekki::backend::vulkan::ImageViewDesc viewDesc) {
    auto& device = api.Device();
    auto& image = api.Resources().image(imageRef);

    auto view = image.GetView(const_cast<tekki::backend::vulkan::Device&>(device), viewDesc);
    auto view_desc = image.GetViewDesc(viewDesc);

    NVSDK_NGX_Resource_VK resource{};
    resource.Resource.ImageViewInfo.ImageView = view;
    resource.Resource.ImageViewInfo.Image = image.Raw;
    resource.Resource.ImageViewInfo.SubresourceRange = {
        view_desc.subresourceRange.aspectMask,
        view_desc.subresourceRange.baseMipLevel,
        view_desc.subresourceRange.levelCount,
        0, // base array layer
        1  // layer count
    };
    resource.Resource.ImageViewInfo.Format = view_desc.format != VK_FORMAT_UNDEFINED ? view_desc.format : image.desc.Format;
    resource.Resource.ImageViewInfo.Width = image.desc.Extent.x;
    resource.Resource.ImageViewInfo.Height = image.desc.Extent.y;
    resource.Type = NVSDK_NGX_Resource_VK_Type_NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    resource.ReadWrite = false;

    return resource;
}

} // namespace

DlssRenderer::DlssOptimalSettings DlssRenderer::DlssOptimalSettings::ForTargetResolutionAtQuality(
    void* ngxParams, const glm::u32vec2& targetResolution, NVSDK_NGX_PerfQuality_Value qualityValue) {
    
    DlssOptimalSettings settings{};
    auto params = static_cast<NVSDK_NGX_Parameter*>(ngxParams);

    void* getOptimalSettingsFn = nullptr;
    DlssRenderer::CheckNgxResult(NVSDK_NGX_Parameter_GetVoidPointer(
        params,
        NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback,
        &getOptimalSettingsFn
    ));

    if (!getOptimalSettingsFn) {
        throw std::runtime_error("DLSS optimal settings callback not found");
    }

    auto callback = reinterpret_cast<PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback>(getOptimalSettingsFn);

    NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_Parameter_Width, targetResolution.x);
    NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_Parameter_Height, targetResolution.y);
    NVSDK_NGX_Parameter_SetI(params, NVSDK_NGX_Parameter_PerfQualityValue, qualityValue);
    NVSDK_NGX_Parameter_SetI(params, NVSDK_NGX_Parameter_RTXValue, 0);

    DlssRenderer::CheckNgxResult(callback(params));

    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_OutWidth, &settings.OptimalRenderExtent.x);
    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_OutHeight, &settings.OptimalRenderExtent.y);
    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, &settings.MaxRenderExtent.x);
    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, &settings.MaxRenderExtent.y);
    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, &settings.MinRenderExtent.x);
    NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, &settings.MinRenderExtent.y);

    return settings;
}

DlssRenderer::DlssRenderer(std::shared_ptr<tekki::backend::RenderBackend> backend,
                          const glm::u32vec2& inputResolution,
                          const glm::u32vec2& targetResolution) {
    
    uint32_t instExtCount = 0;
    const char** instExts = nullptr;
    uint32_t deviceExtCount = 0;
    const char** deviceExts = nullptr;

    DlssRenderer::CheckNgxResult(NVSDK_NGX_VULKAN_RequiredExtensions(
        &instExtCount,
        &instExts,
        &deviceExtCount,
        &deviceExts
    ));

    std::filesystem::path dlssSearchPath = "/kajiya";
    if (!std::filesystem::exists(dlssSearchPath)) {
        throw std::runtime_error("/kajiya VFS entry not found. Did you forget to call `set_standard_vfs_mount_points`?");
    }

    std::wstring dlssSearchPathW = dlssSearchPath.wstring();
    std::vector<const wchar_t*> ngxDllPaths = { dlssSearchPathW.c_str() };

    NVSDK_NGX_FeatureCommonInfo ngxCommonInfo{};
    ngxCommonInfo.PathListInfo.Path = const_cast<wchar_t**>(ngxDllPaths.data());
    ngxCommonInfo.PathListInfo.Length = static_cast<uint32_t>(ngxDllPaths.size());
    ngxCommonInfo.InternalData = nullptr;
    ngxCommonInfo.LoggingInfo.LoggingCallback = nullptr;
    ngxCommonInfo.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_Logging_Level_NVSDK_NGX_LOGGING_LEVEL_OFF;
    ngxCommonInfo.LoggingInfo.DisableOtherLoggingSinks = false;

    auto vulkanDevice = std::dynamic_pointer_cast<tekki::backend::vulkan::Device>(backend->Device);
    if (!vulkanDevice) {
        throw std::runtime_error("Vulkan device required for DLSS");
    }

    auto physicalDevice = vulkanDevice->PhysicalDevice();
    if (!physicalDevice) {
        throw std::runtime_error("Physical device not available");
    }

    DlssRenderer::CheckNgxResult(NVSDK_NGX_VULKAN_Init(
        0xcafebabe,
        L".",
        physicalDevice->instance->GetRaw(),
        physicalDevice->raw,
        vulkanDevice->GetRaw(),
        &ngxCommonInfo,
        NVSDK_NGX_Version_NVSDK_NGX_Version_API
    ));

    NVSDK_NGX_Parameter* ngxParams = nullptr;
    DlssRenderer::CheckNgxResult(NVSDK_NGX_VULKAN_GetCapabilityParameters(&ngxParams));
    NgxParams = ngxParams;

    int supersamplingNeedsUpdatedDriver = 0;
    DlssRenderer::CheckNgxResult(NVSDK_NGX_Parameter_GetI(
        ngxParams,
        NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
        &supersamplingNeedsUpdatedDriver
    ));
    if (supersamplingNeedsUpdatedDriver != 0) {
        throw std::runtime_error("DLSS requires updated driver");
    }

    int supersamplingAvailable = 0;
    DlssRenderer::CheckNgxResult(NVSDK_NGX_Parameter_GetI(
        ngxParams,
        NVSDK_NGX_Parameter_SuperSampling_Available,
        &supersamplingAvailable
    ));
    if (supersamplingAvailable != 1) {
        throw std::runtime_error("DLSS not available");
    }

    std::vector<NVSDK_NGX_PerfQuality_Value> qualityPreferenceOrder = {
        NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxQuality,
        NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_Balanced,
        NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxPerf,
        NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_UltraPerformance
    };

    std::vector<std::pair<NVSDK_NGX_PerfQuality_Value, DlssOptimalSettings>> supportedQualityModes;
    for (auto qualityValue : qualityPreferenceOrder) {
        auto settings = DlssOptimalSettings::ForTargetResolutionAtQuality(ngxParams, targetResolution, qualityValue);
        if (settings.SupportsInputResolution(inputResolution)) {
            supportedQualityModes.emplace_back(qualityValue, settings);
        }
    }

    auto optimalSettings = std::find_if(supportedQualityModes.begin(), supportedQualityModes.end(),
        [&](const auto& mode) {
            return mode.second.OptimalRenderExtent == inputResolution;
        });

    NVSDK_NGX_PerfQuality_Value optimalQualityValue;
    DlssOptimalSettings finalSettings;

    if (optimalSettings != supportedQualityModes.end()) {
        optimalQualityValue = optimalSettings->first;
        finalSettings = optimalSettings->second;
    } else if (!supportedQualityModes.empty()) {
        optimalQualityValue = supportedQualityModes[0].first;
        finalSettings = supportedQualityModes[0].second;
    } else {
        throw std::runtime_error("No DLSS quality mode can produce target resolution from input resolution");
    }

    const char* qualityValueStr = "unknown";
    switch (optimalQualityValue) {
        case NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxPerf:
            qualityValueStr = "MaxPerf";
            break;
        case NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_Balanced:
            qualityValueStr = "Balanced";
            break;
        case NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxQuality:
            qualityValueStr = "MaxQuality";
            break;
        case NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_UltraPerformance:
            qualityValueStr = "UltraPerformance";
            break;
        case NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_UltraQuality:
            qualityValueStr = "UltraQuality";
            break;
    }

    // Log the selected mode
    // TODO: Add proper logging
    // spdlog::info("Using {} DLSS mode", qualityValueStr);

    NVSDK_NGX_DLSS_Create_Params dlssCreateParams{};
    dlssCreateParams.Feature.InWidth = finalSettings.OptimalRenderExtent.x;
    dlssCreateParams.Feature.InHeight = finalSettings.OptimalRenderExtent.y;
    dlssCreateParams.Feature.InTargetWidth = targetResolution.x;
    dlssCreateParams.Feature.InTargetHeight = targetResolution.y;
    dlssCreateParams.Feature.InPerfQualityValue = optimalQualityValue;
    dlssCreateParams.InFeatureCreateFlags =
        NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
        NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
        NVSDK_NGX_DLSS_Feature_Flags_NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    dlssCreateParams.InEnableOutputSubrects = false;

    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_CreationNodeMask, 1);
    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_VisibilityNodeMask, 1);
    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_Width, dlssCreateParams.Feature.InWidth);
    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_Height, dlssCreateParams.Feature.InHeight);
    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_OutWidth, dlssCreateParams.Feature.InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_Parameter_OutHeight, dlssCreateParams.Feature.InTargetHeight);
    NVSDK_NGX_Parameter_SetI(ngxParams, NVSDK_NGX_Parameter_PerfQualityValue, dlssCreateParams.Feature.InPerfQualityValue);
    NVSDK_NGX_Parameter_SetI(ngxParams, NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, dlssCreateParams.InFeatureCreateFlags);
    NVSDK_NGX_Parameter_SetI(ngxParams, NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, dlssCreateParams.InEnableOutputSubrects ? 1 : 0);

    NVSDK_NGX_Handle* dlssFeature = nullptr;

    // Create DLSS feature using a setup command buffer
    // TODO: This needs to be called with a valid command buffer from the backend
    // For now, we'll need to add a method to the backend to execute setup commands
    auto vulkanDevice2 = std::dynamic_pointer_cast<tekki::backend::vulkan::Device>(backend->Device);

    // Execute with setup callback
    vulkanDevice2->WithSetupCb([&](VkCommandBuffer cb) {
        DlssRenderer::CheckNgxResult(NVSDK_NGX_VULKAN_CreateFeature(
            cb,
            NVSDK_NGX_Feature_NVSDK_NGX_Feature_SuperSampling,
            ngxParams,
            &dlssFeature
        ));
    });

    DlssFeature = dlssFeature;
    OptimalSettings = finalSettings;
    Backend = backend;
}

} // namespace tekki::renderer::renderers