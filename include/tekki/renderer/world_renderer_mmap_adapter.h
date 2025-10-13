#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"

// Forward declarations
namespace tekki::renderer {
class WorldRenderer;
class MeshHandle;
struct AddMeshOptions;
} // namespace tekki::renderer

namespace tekki::renderer::mmap_adapter {

// Helper functions for memory-mapped asset loading
template<typename T>
std::shared_ptr<T> MmapMappedAsset(const std::string& path);

} // namespace tekki::renderer::mmap_adapter