#include "tekki/render_graph/types.h"
#include "tekki/render_graph/graph.h"

namespace tekki::render_graph {

ResourceRegistry::ResourceRegistry(
    const RenderGraphExecutionParams* params,
    std::vector<RegistryResource>&& resources,
    DynamicConstants* dynamicConstants,
    const RenderGraphPipelines* pipelines)
    : ExecutionParams(params)
    , Resources(std::move(resources))
    , DynamicConstants_(dynamicConstants)
    , Pipelines(pipelines) {
}

} // namespace tekki::render_graph
