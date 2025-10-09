#include "tekki/render_graph/render_graph.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/render_graph/temporal.h"

namespace tekki::render_graph
{

// Basic implementations for key types

// RenderGraph implementation
RenderGraph::RenderGraph()
{
    // Initialize with empty vectors
    passes = {};
    resources = {};
    exported_resources = {};
    compute_pipelines = {};
    raster_pipelines = {};
    rt_pipelines = {};
    predefined_descriptor_set_layouts = {};
    debug_hook = std::nullopt;
    debugged_resource = std::nullopt;
}

// PassBuilder implementation
PassBuilder::PassBuilder(RenderGraph* rg, size_t pass_idx) : rg(rg), pass_idx(pass_idx)
{
    pass = RecordedPass::new ("", pass_idx);
}

PassBuilder::~PassBuilder()
{
    if (pass)
    {
        rg->record_pass(*pass);
    }
}

// TemporalRenderGraph implementation
TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : device_(device), temporal_state(state)
{
    rg = RenderGraph();
}

} // namespace tekki::render_graph