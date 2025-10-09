#include "tekki/renderer/world_renderer_mmap_adapter.h"
#include <stdexcept>
#include <string>

namespace tekki::renderer {

std::shared_ptr<MeshHandle> WorldRenderer::AddBakedMesh(
    const std::string& path,
    const AddMeshOptions& opts
) {
    try {
        auto mesh = MmapMappedAsset<PackedTriMeshFlat>(path);
        return AddMesh(mesh, opts);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to add baked mesh: " + std::string(e.what()));
    }
}

} // namespace tekki::renderer