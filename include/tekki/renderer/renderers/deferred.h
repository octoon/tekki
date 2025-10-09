#pragma once

#include "renderers.h"
#include "ircache.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Forward declarations
struct IrcacheRenderState;
struct WrcRenderState;

// Light the G-buffer using deferred shading
void light_gbuffer(
    render_graph::RenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& shadow_mask,
    const render_graph::Handle<vulkan::Image>& rtr,
    const render_graph::Handle<vulkan::Image>& rtdgi,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    render_graph::Handle<vulkan::Image>& temporal_output,
    render_graph::Handle<vulkan::Image>& output,
    const render_graph::Handle<vulkan::Image>& sky_cube,
    const render_graph::Handle<vulkan::Image>& convolved_sky_cube,
    vk::DescriptorSet bindless_descriptor_set,
    size_t debug_shading_mode,
    bool debug_show_wrc
);

} // namespace tekki::renderer