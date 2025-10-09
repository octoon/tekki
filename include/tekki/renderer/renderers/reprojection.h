#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../render_graph/temporal.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Calculate reprojection map for temporal algorithms
render_graph::Handle<vulkan::Image> calculate_reprojection_map(
    render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& velocity_img
);

} // namespace tekki::renderer
