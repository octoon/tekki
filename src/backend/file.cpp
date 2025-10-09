#include "tekki/backend/file.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <system_error>
#include <stdexcept>

namespace tekki::backend {

FileWatcher::FileWatcher() {
    // Initialize file watcher with custom delay
    // Note: Actual file watching implementation would need to be provided
}

void FileWatcher::Watch(const std::filesystem::path& path, std::function<void()> callback) {
    // Implementation would depend on the file watching library used
    // This is a placeholder for the actual file watching logic
}

LoadFile::LoadFile(const std::filesystem::path& path) : path_(path) {}

LoadFile::LoadFile(const LoadFile& other) = default;
LoadFile& LoadFile::operator=(const LoadFile& other) = default;

std::vector<uint8_t> LoadFile::Run() {
    // File watching setup - placeholder implementation
    auto& fileWatcher = GetFileWatcher();
    fileWatcher.Watch(path_, []() {
        // Invalidation trigger would be called here
    });
    
    // Read file content
    std::ifstream file(path_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("LoadFile: failed to open file: " + path_.string());
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("LoadFile: failed to read file: " + path_.string());
    }
    
    return buffer;
}

std::string LoadFile::DebugDescription() const {
    return "LoadFile(" + path_.string() + ")";
}

FileWatcher& LoadFile::GetFileWatcher() {
    static FileWatcher fileWatcher;
    return fileWatcher;
}

std::mutex VirtualFileSystem::mountPointsMutex_;
std::unordered_map<std::string, std::filesystem::path> VirtualFileSystem::mountPoints_ = {
    {"/kajiya", "."},
    {"/shaders", "assets/shaders"},
    {"/rust-shaders-compiled", "assets/rust-shaders-compiled"},
    {"/images", "assets/images"},
    {"/cache", "cache"}
};

void VirtualFileSystem::SetVfsMountPoint(const std::string& mountPoint, const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mountPointsMutex_);
    mountPoints_[mountPoint] = path;
}

void VirtualFileSystem::SetStandardVfsMountPoints(const std::filesystem::path& kajiyaPath) {
    SetVfsMountPoint("/kajiya", kajiyaPath);
    SetVfsMountPoint("/shaders", kajiyaPath / "assets/shaders");
    SetVfsMountPoint("/rust-shaders-compiled", kajiyaPath / "assets/rust-shaders-compiled");
    SetVfsMountPoint("/images", kajiyaPath / "assets/images");
}

std::filesystem::path VirtualFileSystem::CanonicalPathFromVfs(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mountPointsMutex_);
    
    for (const auto& [mountPoint, mountedPath] : mountPoints_) {
        std::string pathStr = path.string();
        if (pathStr.find(mountPoint) == 0) {
            std::string relativePath = pathStr.substr(mountPoint.length());
            if (!relativePath.empty() && relativePath[0] == '/') {
                relativePath = relativePath.substr(1);
            }
            
            auto fullPath = mountedPath / relativePath;
            std::error_code ec;
            auto canonicalPath = std::filesystem::canonical(fullPath, ec);
            
            if (ec) {
                throw std::runtime_error(
                    "CanonicalPathFromVfs: failed to canonicalize path: " + 
                    fullPath.string() + " - Mounted parent folder: " + 
                    mountedPath.string() + " - Relative path: " + relativePath
                );
            }
            
            return canonicalPath;
        }
    }
    
    std::string pathStr = path.string();
    if (!pathStr.empty() && pathStr[0] == '/') {
        throw std::runtime_error(
            "CanonicalPathFromVfs: no VFS mount point for: " + pathStr + 
            " - Current mount points available"
        );
    }
    
    return path;
}

std::filesystem::path VirtualFileSystem::NormalizedPathFromVfs(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mountPointsMutex_);
    
    for (const auto& [mountPoint, mountedPath] : mountPoints_) {
        std::string pathStr = path.string();
        if (pathStr.find(mountPoint) == 0) {
            std::string relativePath = pathStr.substr(mountPoint.length());
            if (!relativePath.empty() && relativePath[0] == '/') {
                relativePath = relativePath.substr(1);
            }
            
            auto fullPath = mountedPath / relativePath;
            std::error_code ec;
            auto normalizedPath = std::filesystem::weakly_canonical(fullPath, ec);
            
            if (ec) {
                throw std::runtime_error(
                    "NormalizedPathFromVfs: failed to normalize path: " + 
                    fullPath.string() + " - Mounted parent folder: " + 
                    mountedPath.string() + " - Relative path: " + relativePath
                );
            }
            
            return normalizedPath;
        }
    }
    
    std::string pathStr = path.string();
    if (!pathStr.empty() && pathStr[0] == '/') {
        throw std::runtime_error(
            "NormalizedPathFromVfs: no VFS mount point for: " + pathStr + 
            " - Current mount points available"
        );
    }
    
    return path;
}

} // namespace tekki::backend