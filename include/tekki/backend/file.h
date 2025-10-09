#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tekki::backend {

// Virtual File System (VFS) for mounting and resolving paths
class VirtualFileSystem {
public:
    static VirtualFileSystem& GetInstance();

    // Set a mount point for the VFS
    void SetMountPoint(const std::string& mount_point, const std::filesystem::path& path);

    // Set standard VFS mount points based on kajiya path
    void SetStandardMountPoints(const std::filesystem::path& kajiya_path);

    // Convert a VFS path to a canonical filesystem path
    std::filesystem::path CanonicalPathFromVfs(const std::filesystem::path& vfs_path) const;

    // Convert a VFS path to a normalized filesystem path
    std::filesystem::path NormalizedPathFromVfs(const std::filesystem::path& vfs_path) const;

private:
    VirtualFileSystem();
    std::unordered_map<std::string, std::filesystem::path> mount_points_;
    mutable std::mutex mutex_;
};

// File watcher callback type
using FileWatchCallback = std::function<void(const std::filesystem::path&)>;

// File watcher for monitoring file changes
class FileWatcher {
public:
    static FileWatcher& GetInstance();

    // Watch a file for changes
    void Watch(const std::filesystem::path& path, FileWatchCallback callback);

    // Stop watching a file
    void Unwatch(const std::filesystem::path& path);

    // Update file watcher (call this periodically)
    void Update();

private:
    FileWatcher();

    struct WatchEntry {
        std::filesystem::path path;
        std::filesystem::file_time_type last_write_time;
        FileWatchCallback callback;
    };

    std::vector<WatchEntry> watched_files_;
    mutable std::mutex mutex_;
};

// Load file data from disk
class FileLoader {
public:
    // Load a file and return its contents as bytes
    static std::vector<uint8_t> LoadFile(const std::filesystem::path& path);

    // Load a file from VFS path
    static std::vector<uint8_t> LoadFileFromVfs(const std::filesystem::path& vfs_path);
};

} // namespace tekki::backend
