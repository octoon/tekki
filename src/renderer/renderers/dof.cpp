#include "tekki/renderer/renderers/dof.h"
#include <memory>
#include <stdexcept>

namespace tekki::renderer::renderers {

std::shared_ptr<tekki::backend::vulkan::Image> Dof::Apply(
    std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph,
    std::shared_ptr<tekki::backend::vulkan::Image> input,
    std::shared_ptr<tekki::backend::vulkan::Image> depth
) {
    // TODO: Implement DOF using SimpleRenderPass once the high-level API is available
    // For now, return the input unchanged as a stub implementation
    (void)renderGraph;  // Unused
    (void)depth;        // Unused

    return input;
}

} // namespace tekki::renderer::renderers
