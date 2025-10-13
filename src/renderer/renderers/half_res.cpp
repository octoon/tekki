#include "tekki/renderer/renderers/half_res.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include <memory>
#include <stdexcept>

namespace tekki::renderer::renderers::half_res {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

/**
 * Extract half resolution GBuffer view normal as RGBA8
 * Stub implementation - TODO: Implement with SimpleRenderPass
 */
rg::Handle<Image> ExtractHalfResGbufferViewNormalRGBA8(
    [[maybe_unused]] rg::RenderGraph& rg,
    [[maybe_unused]] const rg::Handle<Image>& gbuffer
) {
    // TODO: Implement once SimpleRenderPass API is available
    // Original Rust: extract_half_res_gbuffer_view_normal_rgba8
    // Should create half-res RGBA8_SNORM image and extract view normals from gbuffer
    throw std::runtime_error("ExtractHalfResGbufferViewNormalRGBA8 not yet implemented");
}

/**
 * Extract half resolution depth
 * Stub implementation - TODO: Implement with SimpleRenderPass
 */
rg::Handle<Image> ExtractHalfResDepth(
    [[maybe_unused]] rg::RenderGraph& rg,
    [[maybe_unused]] const rg::Handle<Image>& depth
) {
    // TODO: Implement once SimpleRenderPass API is available
    // Original Rust: extract_half_res_depth
    // Should create half-res R32_SFLOAT image and extract depth values
    throw std::runtime_error("ExtractHalfResDepth not yet implemented");
}

} // namespace tekki::renderer::renderers::half_res
