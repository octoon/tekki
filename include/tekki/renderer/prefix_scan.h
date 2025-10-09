#pragma once

#include "../render_graph/RenderGraph.h"
#include "../backend/vulkan/buffer.h"

namespace tekki::renderer {

// Inclusive prefix scan for uint32 arrays up to 1M elements
// This implements a multi-pass GPU prefix scan algorithm using segments
void inclusive_prefix_scan_u32_1m(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Buffer>& input_buf
);

} // namespace tekki::renderer