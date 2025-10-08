#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class IblRenderer {
public:
    IblRenderer();

    render_graph::Handle<vulkan::Image> render(
        render_graph::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const std::array<uint32_t, 2>& output_extent
    );

private:
    // IBL state
    std::shared_ptr<vulkan::Buffer> ibl_constants_buffer_;
    std::shared_ptr<vulkan::Image> environment_map_;
};

} // namespace tekki::renderer