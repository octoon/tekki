#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "ngx_dlss.h"
#include "tekki/backend/render_backend.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/temporal.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

class DlssRenderer {
public:
    struct DlssOptimalSettings {
        glm::u32vec2 OptimalRenderExtent;
        glm::u32vec2 MaxRenderExtent;
        glm::u32vec2 MinRenderExtent;

        bool SupportsInputResolution(const glm::u32vec2& inputResolution) const {
            return inputResolution.x >= MinRenderExtent.x &&
                   inputResolution.y >= MinRenderExtent.y &&
                   inputResolution.x <= MaxRenderExtent.x &&
                   inputResolution.y <= MaxRenderExtent.y;
        }

        static DlssOptimalSettings ForTargetResolutionAtQuality(void* ngxParams, const glm::u32vec2& targetResolution, NVSDK_NGX_PerfQuality_Value qualityValue);
    };

private:
    void* DlssFeature;
    void* NgxParams;
    glm::vec2 CurrentSupersampleOffset;
    uint32_t FrameIndex;
    DlssOptimalSettings OptimalSettings;
    std::shared_ptr<tekki::backend::RenderBackend> Backend;

    static void CheckNgxResult(NVSDK_NGX_Result result) {
        if (result != NVSDK_NGX_Result_NVSDK_NGX_Result_Success) {
            throw std::runtime_error("DLSS operation failed");
        }
    }

    static NVSDK_NGX_Resource_VK ImageToNgx(rg::RenderPassApi& api,
                                           rg::Ref<Image, rg::GpuSrv> imageRef,
                                           tekki::backend::vulkan::ImageViewDesc viewDesc);

    static NVSDK_NGX_Result NgxVulkanEvaluateDlssExt(VkCommandBuffer commandBuffer,
                                                    void* handle,
                                                    void* parameters,
                                                    const NVSDK_NGX_VK_DLSS_Eval_Params& dlssEvalParams);

public:
    DlssRenderer(std::shared_ptr<tekki::backend::RenderBackend> backend,
                 const glm::u32vec2& inputResolution,
                 const glm::u32vec2& targetResolution);

    rg::Handle<Image> Render(
        rg::TemporalRenderGraph& renderGraph,
        const rg::Handle<Image>& input,
        const rg::Handle<Image>& reprojectionMap,
        const rg::Handle<Image>& depth,
        const glm::u32vec2& outputExtent);

    glm::vec2 GetCurrentSupersampleOffset() const { return CurrentSupersampleOffset; }
    void SetCurrentSupersampleOffset(const glm::vec2& offset) { CurrentSupersampleOffset = offset; }
};

} // namespace tekki::renderer::renderers