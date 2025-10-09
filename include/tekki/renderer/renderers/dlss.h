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
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/render_graph.h"

namespace tekki::renderer::renderers {

class DlssRenderer {
private:
    void* DlssFeature;
    void* NgxParams;
    glm::vec2 CurrentSupersampleOffset;
    uint32_t FrameIndex;

    static void CheckNgxResult(NVSDK_NGX_Result result) {
        if (result != NVSDK_NGX_Result_NVSDK_NGX_Result_Success) {
            throw std::runtime_error("DLSS operation failed");
        }
    }

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

    static NVSDK_NGX_Resource_VK ImageToNgx(tekki::render_graph::RenderPassApi& api, 
                                           tekki::render_graph::Ref<tekki::backend::vulkan::Image> imageRef,
                                           tekki::backend::vulkan::ImageViewDesc viewDesc);

    static NVSDK_NGX_Result NgxVulkanEvaluateDlssExt(VkCommandBuffer commandBuffer,
                                                    void* handle,
                                                    void* parameters,
                                                    const NVSDK_NGX_VK_DLSS_Eval_Params& dlssEvalParams);

public:
    DlssRenderer(std::shared_ptr<tekki::backend::RenderBackend> backend,
                 const glm::u32vec2& inputResolution,
                 const glm::u32vec2& targetResolution);

    tekki::render_graph::Handle<tekki::backend::vulkan::Image> Render(
        tekki::render_graph::TemporalRenderGraph& renderGraph,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& depth,
        const glm::u32vec2& outputExtent);

    glm::vec2 GetCurrentSupersampleOffset() const { return CurrentSupersampleOffset; }
    void SetCurrentSupersampleOffset(const glm::vec2& offset) { CurrentSupersampleOffset = offset; }
};

} // namespace tekki::renderer::renderers