#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class ShadowsRenderer {
public:
    ShadowsRenderer();

    render_graph::Handle<vulkan::Image> render(
        render_graph::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const std::array<uint32_t, 2>& output_extent
    );

private:
    // Shadow rendering state
    std::shared_ptr<vulkan::Buffer> shadow_constants_buffer_;
};

} // namespace tekki::renderer