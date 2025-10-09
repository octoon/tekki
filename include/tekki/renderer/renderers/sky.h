#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Render sky cube map
render_graph::Handle<vulkan::Image> render_sky_cube(
    render_graph::RenderGraph& rg
);

// Convolve cube map for diffuse IBL
render_graph::Handle<vulkan::Image> convolve_cube(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input
);

} // namespace tekki::renderer