#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class RtrRenderer {
public:
    RtrRenderer();

    render_graph::ReadOnlyHandle<vulkan::Image> render(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& prev_radiance,
        vk::DescriptorSet bindless_descriptor_set
    );

private:
    PingPongTemporalResource rtr_tex_;

    void filter_rtr(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        PingPongTemporalResource& temporal_tex
    );
};

} // namespace tekki::renderer