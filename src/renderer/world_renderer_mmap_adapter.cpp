#include "tekki/renderer/world_renderer_mmap_adapter.h"
#include "tekki/renderer/world_renderer.h"
#include "tekki/renderer/mmap.h"
#include <stdexcept>
#include <string>

namespace tekki::renderer::mmap_adapter {

// Stub implementation for memory-mapped asset loading
template<typename T>
std::shared_ptr<T> MmapMappedAsset(const std::string& path) {
    throw std::runtime_error("MmapMappedAsset not yet implemented");
}

} // namespace tekki::renderer::mmap_adapter