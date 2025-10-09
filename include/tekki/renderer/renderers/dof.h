#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

namespace tekki::renderer {

// Depth of Field (DOF) effect renderer
// Implements circle of confusion based depth of field blur
class DofRenderer {
public:
    // Apply depth of field effect to input image
    // Parameters:
    //   rg: Render graph for resource management
    //   input: Input color image
    //   depth: Depth buffer for DOF calculations
    // Returns: DOF processed image
    static render_graph::Handle<vulkan::Image> render_dof(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth
    );

private:
    // Calculate circle of confusion (COC) from depth
    static render_graph::Handle<vulkan::Image> calculate_coc(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& depth,
        const render_graph::Handle<vulkan::Image>& input
    );

    // Create tiled COC for efficient sampling
    static render_graph::Handle<vulkan::Image> create_coc_tiles(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& coc
    );

    // Gather DOF blur using COC and tiles
    static render_graph::Handle<vulkan::Image> gather_dof(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth,
        const render_graph::Handle<vulkan::Image>& coc,
        const render_graph::Handle<vulkan::Image>& coc_tiles
    );
};

} // namespace tekki::renderer