#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "world_renderer.h"

namespace tekki::renderer {

class WorldRenderer {
public:
    /**
     * Add a baked mesh to the world renderer
     * @param path Path to the mesh file
     * @param opts Options for adding the mesh
     * @return Handle to the added mesh
     */
    std::shared_ptr<MeshHandle> AddBakedMesh(
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

private:
    std::shared_ptr<MeshHandle> AddMesh(
        const std::shared_ptr<PackedTriMeshFlat>& mesh,
        const AddMeshOptions& opts
    );

    template<typename T>
    std::shared_ptr<T> MmapMappedAsset(const std::string& path);
};

} // namespace tekki::renderer