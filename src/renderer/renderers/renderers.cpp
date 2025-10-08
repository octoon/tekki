#include "renderers.h"
#include "../../backend/vulkan/image.h"

namespace tekki::renderer {

GbufferDepth::GbufferDepth(
    render_graph::Handle<vulkan::Image> geometric_normal,
    render_graph::Handle<vulkan::Image> gbuffer,
    render_graph::Handle<vulkan::Image> depth
) : geometric_normal(geometric_normal), gbuffer(gbuffer), depth(depth) {}

render_graph::Handle<vulkan::Image> GbufferDepth::get_half_view_normal(render_graph::RenderGraph& rg) {
    if (!half_view_normal.has_value()) {
        // Create half-resolution view normal
        auto desc = rg.resource_desc(geometric_normal);
        desc.extent[0] /= 2;
        desc.extent[1] /= 2;

        half_view_normal = rg.create_image(desc, "half_view_normal");
    }
    return half_view_normal.value();
}

render_graph::Handle<vulkan::Image> GbufferDepth::get_half_depth(render_graph::RenderGraph& rg) {
    if (!half_depth.has_value()) {
        // Create half-resolution depth
        auto desc = rg.resource_desc(depth);
        desc.extent[0] /= 2;
        desc.extent[1] /= 2;

        half_depth = rg.create_image(desc, "half_depth");
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