#include "tekki/renderer/renderers/reference.h"
#include <stdexcept>

namespace tekki::renderer::renderers {

void ReferencePathTrace::Execute(
    [[maybe_unused]] rg::RenderGraph& renderGraph,
    [[maybe_unused]] rg::Handle<Image>& outputImage,
    [[maybe_unused]] VkDescriptorSet bindlessDescriptorSet,
    [[maybe_unused]] const rg::Handle<RayTracingAcceleration>& tlas
) {
    // TODO: Implement reference path tracing once SimpleRenderPass and ShaderSource are available
    // Original Rust: kajiya/src/renderers/reference.rs
    // Requires:
    // - SimpleRenderPass for ray tracing
    // - ShaderSource for shader loading
    throw std::runtime_error("ReferencePathTrace::Execute not yet implemented");
}

} // namespace tekki::renderer::renderers