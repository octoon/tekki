#pragma once

#include "tekki/render_graph/graph.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer::renderers::half_res {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

/**
 * Extract half resolution GBuffer view normal as RGBA8
 */
rg::Handle<Image> ExtractHalfResGbufferViewNormalRGBA8(
    rg::RenderGraph& rg,
    const rg::Handle<Image>& gbuffer
);

/**
 * Extract half resolution depth
 */
rg::Handle<Image> ExtractHalfResDepth(
    rg::RenderGraph& rg,
    const rg::Handle<Image>& depth
);

} // namespace tekki::renderer::renderers::half_res
