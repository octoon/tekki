#include "tekki/renderer/renderers/deferred.h"
#include <stdexcept>

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

/**
 * Light GBuffer rendering pass
 * Stub - TODO: Implement with SimpleRenderPass
 */
void LightGbuffer(
    [[maybe_unused]] rg::RenderGraph& rg,
    [[maybe_unused]] const GbufferDepth& gbufferDepth,
    [[maybe_unused]] const rg::Handle<Image>& shadowMask,
    [[maybe_unused]] const rg::Handle<Image>& rtr,
    [[maybe_unused]] const rg::Handle<Image>& rtdgi,
    [[maybe_unused]] IrcacheRenderState& ircache,
    [[maybe_unused]] const WrcRenderState& wrc,
    [[maybe_unused]] rg::Handle<Image>& temporalOutput,
    [[maybe_unused]] rg::Handle<Image>& output,
    [[maybe_unused]] const rg::Handle<Image>& skyCube,
    [[maybe_unused]] const rg::Handle<Image>& convolvedSkyCube,
    [[maybe_unused]] vk::DescriptorSet bindlessDescriptorSet,
    [[maybe_unused]] size_t debugShadingMode,
    [[maybe_unused]] bool debugShowWrc
) {
    // TODO: Implement once SimpleRenderPass API is available
    // Original Rust: kajiya/src/renderers/deferred.rs::light_gbuffer
    throw std::runtime_error("LightGbuffer not yet implemented");
}

} // namespace tekki::renderer::renderers
