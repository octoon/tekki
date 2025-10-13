#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/Image.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

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
    rg::Handle<rg::Image> Render(
        rg::TemporalRenderGraph& rg,
        const GbufferDepth& gbufferDepth,
        const rg::Handle<rg::Image>& shadowMask,
        const rg::Handle<rg::Image>& reprojectionMap
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
    static void FilterSpatial(
        rg::TemporalRenderGraph& rg,
        uint32_t stepSize,
        const rg::Handle<rg::Image>& inputImage,
        rg::Handle<rg::Image>& outputImage,
        const rg::Handle<rg::Image>& metadataImage,
        const GbufferDepth& gbufferDepth,
        glm::uvec2 bitpackedShadowMaskExtent
    );

    PingPongTemporalResource accum_;
    PingPongTemporalResource moments_;
};

} // namespace tekki::renderer::renderers