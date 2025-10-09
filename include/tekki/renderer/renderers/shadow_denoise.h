#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/renderer/render_graph/temporal_render_graph.h"
#include "tekki/renderer/resources/gbuffer_depth.h"
#include "tekki/renderer/resources/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

class ShadowDenoiseRenderer {
public:
    ShadowDenoiseRenderer();
    
    /**
     * Render shadow denoising pass
     * @param rg Temporal render graph
     * @param gbufferDepth G-buffer and depth information
     * @param shadowMask Shadow mask input
     * @param reprojectionMap Reprojection map for temporal accumulation
     * @return Denoised shadow output
     */
    std::shared_ptr<tekki::renderer::Image> Render(
        std::shared_ptr<tekki::renderer::TemporalRenderGraph> rg,
        std::shared_ptr<tekki::renderer::GbufferDepth> gbufferDepth,
        std::shared_ptr<tekki::renderer::Image> shadowMask,
        std::shared_ptr<tekki::renderer::Image> reprojectionMap
    );

private:
    /**
     * Apply spatial filtering to shadow data
     * @param rg Temporal render graph
     * @param stepSize Filter step size
     * @param inputImage Input image to filter
     * @param outputImage Output filtered image
     * @param metadataImage Metadata for filtering
     * @param gbufferDepth G-buffer and depth information
     * @param bitpackedShadowMaskExtent Extent of bitpacked shadow mask
     */
    void FilterSpatial(
        std::shared_ptr<tekki::renderer::TemporalRenderGraph> rg,
        uint32_t stepSize,
        std::shared_ptr<tekki::renderer::Image> inputImage,
        std::shared_ptr<tekki::renderer::Image> outputImage,
        std::shared_ptr<tekki::renderer::Image> metadataImage,
        std::shared_ptr<tekki::renderer::GbufferDepth> gbufferDepth,
        glm::uvec2 bitpackedShadowMaskExtent
    );

    PingPongTemporalResource accum_;
    PingPongTemporalResource moments_;
};

} // namespace tekki::renderer::renderers