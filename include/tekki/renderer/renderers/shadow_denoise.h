#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class ShadowDenoiseRenderer {
public:
    ShadowDenoiseRenderer();

    render_graph::Handle<vulkan::Image> denoise_shadows(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& shadow_map,
        const GbufferDepth& gbuffer_depth,
        const std::array<uint32_t, 2>& output_extent
    );

private:
    // Shadow denoising state
    std::shared_ptr<vulkan::Buffer> shadow_denoise_constants_buffer_;
};

} // namespace tekki::renderer