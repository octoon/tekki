#pragma once

#include <filesystem>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tekki::core {

// Cross-platform memory-mapped file wrapper
class MemoryMappedFile {
public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile();

    // Move-only type
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

    // Open file for memory mapping
    bool open(const std::filesystem::path& file_path);

    // Close memory mapped file
    void close();

    // Get pointer to mapped data
    const void* data() const { return data_; }

    // Get size of mapped data
    size_t size() const { return size_; }

    // Get typed pointer to mapped data
    template<typename T>
    const T* as() const {
        if (size_ < sizeof(T)) {
            throw std::runtime_error("Memory mapped file too small for requested type");
        }
        return static_cast<const T*>(data_);
    }

    // Check if file is currently mapped
    bool is_open() const { return data_ != nullptr; }

private:
    const void* data_ = nullptr;
    size_t size_ = 0;

#ifdef _WIN32
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle_ = INVALID_HANDLE_VALUE;
#else
    int file_descriptor_ = -1;
#endif

    void cleanup();
};

// Asset memory mapping cache
// Provides cached access to memory-mapped asset files
class AssetMmapCache {
public:
    // Get singleton instance
    static AssetMmapCache& instance();

    // Get memory-mapped asset as typed pointer
    // Template parameter T: Type to cast the mapped data to
    // Parameter path: File path to memory map
    // Returns: Typed pointer to memory-mapped data
    // Throws: std::runtime_error if file cannot be mapped or is wrong size
    template<typename T>
    const T* get_mmapped_asset(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = mmaps_.find(path);
        if (it == mmaps_.end()) {
            // Create new memory mapping
            auto mmap = std::make_unique<MemoryMappedFile>();
            if (!mmap->open(path)) {
                throw std::runtime_error("Failed to memory map file: " + path.string());
            }

            const T* result = mmap->as<T>();
            mmaps_[path] = std::move(mmap);
            return result;
        } else {
            // Return cached mapping
            return it->second->as<T>();
        }
    }

    // Clear all cached memory mappings
    void clear();

private:
    AssetMmapCache() = default;

    std::mutex mutex_;
    std::unordered_map<std::filesystem::path, std::unique_ptr<MemoryMappedFile>> mmaps_;
};

// Convenience function for memory-mapped asset access
// Template parameter T: Type to cast the mapped data to
// Parameter path: File path to memory map
// Returns: Typed pointer to memory-mapped data
template<typename T>
const T* mmapped_asset(const std::filesystem::path& path) {
    return AssetMmapCache::instance().get_mmapped_asset<T>(path);
}

} // namespace tekki::core