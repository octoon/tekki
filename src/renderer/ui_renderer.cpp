#include "tekki/renderer/ui_renderer.h"
#include <stdexcept>

namespace tekki::renderer {

namespace rg = tekki::render_graph;

UiRenderer::UiRenderer() : UiFrame(std::nullopt) {}

/**
 * Prepare render graph for UI rendering
 * Stub - TODO: Implement once TemporalRenderGraph API is complete
 */
rg::Handle<Image> UiRenderer::PrepareRenderGraph([[maybe_unused]] rg::TemporalRenderGraph& rg) {
    // TODO: Implement UI rendering once:
    // - TemporalRenderGraph is fully implemented
    // - RenderGraph::Import is available
    // - PassBuilder::Raster is available
    // - CommandBuffer::Raw is available
    throw std::runtime_error("UiRenderer::PrepareRenderGraph not yet implemented");
}

/**
 * Render UI to the render graph
 * Stub - TODO: Implement with proper render graph support
 */
rg::Handle<Image> UiRenderer::RenderUi([[maybe_unused]] rg::RenderGraph& rg) {
    throw std::runtime_error("UiRenderer::RenderUi not yet implemented");
}

} // namespace tekki::renderer
