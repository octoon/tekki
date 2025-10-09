#include "tekki/renderer/mmap.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <stdexcept>

namespace tekki::renderer {

std::unordered_map<std::filesystem::path, std::vector<uint8_t>> AssetMmapManager::AssetMmaps;
std::mutex AssetMmapManager::AssetMmapsMutex;

/**
 * Get a memory-mapped asset from the given path
 * @param path The path to the asset file
 * @return Pointer to the memory-mapped asset data
 * @throws std::runtime_error if the file cannot be opened or memory-mapped
 */
template<typename T>
const T* AssetMmapManager::MmappedAsset(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(AssetMmapsMutex);
    
    auto canonicalPath = std::filesystem::canonical(path);
    
    auto it = AssetMmaps.find(canonicalPath);
    if (it == AssetMmaps.end()) {
        std::ifstream file(canonicalPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Could not mmap " + canonicalPath.string() + ": file cannot be opened");
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Could not mmap " + canonicalPath.string() + ": failed to read file");
        }
        
        it = AssetMmaps.emplace(canonicalPath, std::move(buffer)).first;
    }
    
    const std::vector<uint8_t>& data = it->second;
    if (data.size() < sizeof(T)) {
        throw std::runtime_error("Memory mapped data size is smaller than expected type size");
    }
    
    return reinterpret_cast<const T*>(data.data());
}

} // namespace tekki::renderer