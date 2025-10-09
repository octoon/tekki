#include "../../include/tekki/renderer/renderers/renderers.h"
#include "../../backend/vulkan/image.h"
#include "../../include/tekki/renderer/half_res.h"

namespace tekki::renderer {

GbufferDepth::GbufferDepth(
    render_graph::Handle<vulkan::Image> geometric_normal,
    render_graph::Handle<vulkan::Image> gbuffer,
    render_graph::Handle<vulkan::Image> depth
) : geometric_normal(geometric_normal), gbuffer(gbuffer), depth(depth) {}

render_graph::Handle<vulkan::Image> GbufferDepth::get_half_view_normal(render_graph::RenderGraph& rg) {
    if (!half_view_normal.has_value()) {
        // Use the dedicated half_res utility function
        half_view_normal = extract_half_res_gbuffer_view_normal_rgba8(rg, gbuffer);
    }
    return half_view_normal.value();
}

render_graph::Handle<vulkan::Image> GbufferDepth::get_half_depth(render_graph::RenderGraph& rg) {
    if (!half_depth.has_value()) {
        // Use the dedicated half_res utility function
        half_depth = extract_half_res_depth(rg, depth);
    }
    return half_depth.value();
}

PingPongTemporalResource::PingPongTemporalResource(const std::string& name)
    : output_tex(name + "_output")
    , history_tex(name + "_history") {}

std::pair<
    render_graph::Handle<vulkan::Image>,
    render_graph::Handle<vulkan::Image>
> PingPongTemporalResource::get_output_and_history(
    render_graph::TemporalRenderGraph& rg,
    const vulkan::ImageDesc& desc
) {
    auto output = rg.get_temporal_resource(output_tex, desc);
    auto history = rg.get_temporal_resource(history_tex, desc);
    return {output, history};
}

} // namespace tekki::renderer