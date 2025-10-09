#pragma once

#include "../render_graph/RenderGraph.h"
#include "../backend/vulkan/image.h"

namespace tekki::renderer {

// Extract half-resolution G-buffer view normal in RGBA8 format
render_graph::Handle<vulkan::Image> extract_half_res_gbuffer_view_normal_rgba8(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& gbuffer
);

// Extract half-resolution depth in R32 format
render_graph::Handle<vulkan::Image> extract_half_res_depth(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& depth
);

} // namespace tekki::renderer