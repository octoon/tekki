#include "tekki/renderer/renderers/raster_meshes.h"
#include <stdexcept>

namespace tekki::renderer::renderers {

void RasterMeshes::Render(
    [[maybe_unused]] rg::RenderGraph& renderGraph,
    [[maybe_unused]] rg::PassBuilder& renderPass,
    [[maybe_unused]] GbufferDepth& gbufferDepth,
    [[maybe_unused]] rg::Handle<Image>& velocityImage,
    [[maybe_unused]] const RasterMeshesData& meshData
) {
    // TODO: Implement raster meshes rendering once RenderPassApi is complete
    // Original Rust: kajiya/src/renderers/raster_meshes.rs
    // Requires:
    // - PassBuilder::RegisterRasterPipeline
    // - PassBuilder::Raster for color attachments
    // - RenderPassApi with proper rasterization support
    throw std::runtime_error("RasterMeshes::Render not yet implemented");
}

} // namespace tekki::renderer::renderers
