#include "tekki/render_graph/temporal.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/buffer.h"
#include <variant>
#include <stdexcept>

namespace tekki::render_graph {

// TemporalRenderGraph constructor
TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : rg(), device_(std::move(device)), temporal_state(std::move(state)) {}

// Simplified implementations for now
std::tuple<RenderGraph, ExportedTemporalRenderGraphState>
TemporalRenderGraph::finish(RetiredRenderGraph rg_moved) {
    TemporalRenderGraphState state_moved = std::move(temporal_state);
    ExportedTemporalRenderGraphState exported_state{.state = std::move(state_moved)};
    return {std::move(rg_moved).rg, std::move(exported_state)};
}

// Placeholder implementations
TemporalRenderGraphState ExportedTemporalRenderGraphState::close(const RetiredRenderGraph& rg) const {
    return state;
}

} // namespace tekki::render_graph