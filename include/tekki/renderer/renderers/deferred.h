#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/renderer/render_graph/render_graph.h"
#include "tekki/renderer/render_graph/simple_render_pass.h"
#include "tekki/renderer/renderers/ircache_render_state.h"
#include "tekki/renderer/renderers/wrc_render_state.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"

namespace tekki::renderer::renderers {

/**
 * Light GBuffer rendering pass
 */
void LightGbuffer(
    RenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<Image>& shadowMask,
    const std::shared_ptr<Image>& rtr,
    const std::shared_ptr<Image>& rtdgi,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    std::shared_ptr<Image>& temporalOutput,
    std::shared_ptr<Image>& output,
    const std::shared_ptr<Image>& skyCube,
    const std::shared_ptr<Image>& convolvedSkyCube,
    vk::DescriptorSet bindlessDescriptorSet,
    size_t debugShadingMode,
    bool debugShowWrc
);

} // namespace tekki::renderer::renderers