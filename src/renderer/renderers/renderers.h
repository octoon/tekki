#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <memory>
#include <optional>

namespace tekki::renderer {

// Forward declarations for renderers
class PostProcessRenderer;
class SsgiRenderer;
class RtrRenderer;
class LightingRenderer;
class IrcacheRenderer;
class RtdgiRenderer;
class TaaRenderer;
class ShadowDenoiseRenderer;
class IblRenderer;

// G-buffer and depth structure
struct GbufferDepth {
    render_graph::Handle<vulkan::Image> geometric_normal;
    render_graph::Handle<vulkan::Image> gbuffer;
    render_graph::Handle<vulkan::Image> depth;

    std::optional<render_graph::Handle<vulkan::Image>> half_view_normal;
    std::optional<render_graph::Handle<vulkan::Image>> half_depth;

    GbufferDepth(
        render_graph::Handle<vulkan::Image> geometric_normal,
        render_graph::Handle<vulkan::Image> gbuffer,
        render_graph::Handle<vulkan::Image> depth
    );

    render_graph::Handle<vulkan::Image> get_half_view_normal(render_graph::RenderGraph& rg);
    render_graph::Handle<vulkan::Image> get_half_depth(render_graph::RenderGraph& rg);
};

// Ping-pong temporal resource
struct PingPongTemporalResource {
    render_graph::TemporalResourceKey output_tex;
    render_graph::TemporalResourceKey history_tex;

    PingPongTemporalResource(const std::string& name);

    std::pair<
        render_graph::Handle<vulkan::Image>,
        render_graph::Handle<vulkan::Image>
    > get_output_and_history(
        render_graph::TemporalRenderGraph& rg,
        const vulkan::ImageDesc& desc
    );
};

// Image LUT
struct ImageLut {
    // TODO: Implement ImageLut structure
};

} // namespace tekki::renderer