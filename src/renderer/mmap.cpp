#include "tekki/renderer/mmap.h"

namespace tekki::renderer {

// Static member definitions
std::unordered_map<std::filesystem::path, std::vector<uint8_t>> AssetMmapManager::AssetMmaps;
std::mutex AssetMmapManager::AssetMmapsMutex;

} // namespace tekki::renderer
