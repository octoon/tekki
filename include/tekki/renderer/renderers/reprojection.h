#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class ReprojectionRenderer {
public:
    ReprojectionRenderer();

    render_graph::Handle<vulkan::Image> render(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& velocity,
        const std::array<uint32_t, 2>& output_extent
    );

private:
    // Reprojection state
    std::shared_ptr<vulkan::Buffer> reprojection_constants_buffer_;
};

} // namespace tekki::renderer