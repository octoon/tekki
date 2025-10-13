#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/renderer/renderers/ircache_render_state.h"
#include "tekki/renderer/renderers/wrc_render_state.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

/**
 * Light GBuffer rendering pass
 * Stub - TODO: Implement with SimpleRenderPass
 */
void LightGbuffer(
    rg::RenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& shadowMask,
    const rg::Handle<Image>& rtr,
    const rg::Handle<Image>& rtdgi,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    rg::Handle<Image>& temporalOutput,
    rg::Handle<Image>& output,
    const rg::Handle<Image>& skyCube,
    const rg::Handle<Image>& convolvedSkyCube,
    vk::DescriptorSet bindlessDescriptorSet,
    size_t debugShadingMode,
    bool debugShowWrc
);

} // namespace tekki::renderer::renderers
