#include "tekki/backend/file.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>

namespace tekki::backend {

// VirtualFileSystem implementation
VirtualFileSystem& VirtualFileSystem::GetInstance() {
    static VirtualFileSystem instance;
    return instance;
}

VirtualFileSystem::VirtualFileSystem() {
    // Set default mount points
    mount_points_["/kajiya"] = ".";
    mount_points_["/shaders"] = "assets/shaders";
    mount_points_["/rust-shaders-compiled"] = "assets/rust-shaders-compiled";
    mount_points_["/images"] = "assets/images";
    mount_points_["/cache"] = "cache";
}

void VirtualFileSystem::SetMountPoint(const std::string& mount_point, const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    mount_points_[mount_point] = path;
}

void VirtualFileSystem::SetStandardMountPoints(const std::filesystem::path& kajiya_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    mount_points_["/kajiya"] = kajiya_path;
    mount_points_["/shaders"] = kajiya_path / "assets" / "shaders";
    mount_points_["/rust-shaders-compiled"] = kajiya_path / "assets" / "rust-shaders-compiled";
    mount_points_["/images"] = kajiya_path / "assets" / "images";
}

std::filesystem::path VirtualFileSystem::CanonicalPathFromVfs(const std::filesystem::path& vfs_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string path_str = vfs_path.string();

    // Check each mount point
    for (const auto& [mount_point, mounted_path] : mount_points_) {
        if (path_str.starts_with(mount_point)) {
            // Get the relative path after the mount point
            std::string rel_path_str = path_str.substr(mount_point.length());
            std::filesystem::path rel_path(rel_path_str);

            // Join with mounted path and canonicalize
            std::filesystem::path result = mounted_path / rel_path;

            std::error_code ec;
            auto canonical = std::filesystem::canonical(result, ec);
            if (ec) {
                std::ostringstream oss;
                oss << "Failed to canonicalize path: " << result.string()
                    << " (Mounted parent: " << mounted_path.string()
                    << ", Relative path: " << rel_path.string() << ")";
                throw std::runtime_error(oss.str());
            }

            return canonical;
        }
    }

    // Check if path starts with "/" (indicating a VFS path without mount point)
    if (path_str.starts_with("/")) {
        std::ostringstream oss;
        oss << "No VFS mount point for: " << vfs_path.string() << "\nCurrent mount points:\n";
        for (const auto& [mp, p] : mount_points_) {
            oss << "  " << mp << " -> " << p.string() << "\n";
        }
        throw std::runtime_error(oss.str());
    }

    // Not a VFS path, return as-is
    return vfs_path;
}

std::filesystem::path VirtualFileSystem::NormalizedPathFromVfs(const std::filesystem::path& vfs_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string path_str = vfs_path.string();

    // Check each mount point
    for (const auto& [mount_point, mounted_path] : mount_points_) {
        if (path_str.starts_with(mount_point)) {
            // Get the relative path after the mount point
            std::string rel_path_str = path_str.substr(mount_point.length());
            std::filesystem::path rel_path(rel_path_str);

            // Join with mounted path and normalize
            std::filesystem::path result = mounted_path / rel_path;
            return result.lexically_normal();
        }
    }

    // Check if path starts with "/" (indicating a VFS path without mount point)
    if (path_str.starts_with("/")) {
        std::ostringstream oss;
        oss << "No VFS mount point for: " << vfs_path.string() << "\nCurrent mount points:\n";
        for (const auto& [mp, p] : mount_points_) {
            oss << "  " << mp << " -> " << p.string() << "\n";
        }
        throw std::runtime_error(oss.str());
    }

    // Not a VFS path, return as-is normalized
    return vfs_path.lexically_normal();
}

// FileWatcher implementation
FileWatcher& FileWatcher::GetInstance() {
    static FileWatcher instance;
    return instance;
}

FileWatcher::FileWatcher() = default;

void FileWatcher::Watch(const std::filesystem::path& path, FileWatchCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::error_code ec;
    auto last_write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to watch file: " + path.string());
    }

    watched_files_.push_back(WatchEntry{path, last_write_time, callback});
}

void FileWatcher::Unwatch(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    watched_files_.erase(
        std::remove_if(watched_files_.begin(), watched_files_.end(),
                      [&path](const WatchEntry& entry) { return entry.path == path; }),
        watched_files_.end());
}

void FileWatcher::Update() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : watched_files_) {
        std::error_code ec;
        auto current_write_time = std::filesystem::last_write_time(entry.path, ec);

        if (!ec && current_write_time != entry.last_write_time) {
            entry.last_write_time = current_write_time;
            if (entry.callback) {
                entry.callback(entry.path);
            }
        }
    }
}

// FileLoader implementation
std::vector<uint8_t> FileLoader::LoadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }

    return buffer;
}

std::vector<uint8_t> FileLoader::LoadFileFromVfs(const std::filesystem::path& vfs_path) {
    auto canonical_path = VirtualFileSystem::GetInstance().CanonicalPathFromVfs(vfs_path);
    return LoadFile(canonical_path);
}

} // namespace tekki::backend
