#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "tekki/core/result.h"

namespace tekki::backend
{

class FileWatcher
{
public:
    FileWatcher();
    void Watch(const std::filesystem::path& path, std::function<void()> callback);
};

class LoadFile
{
private:
    std::filesystem::path path_;
    static FileWatcher& GetFileWatcher();

public:
    LoadFile(const std::filesystem::path& path);
    LoadFile(const LoadFile& other);
    LoadFile& operator=(const LoadFile& other);

    std::vector<uint8_t> Run();
    std::string DebugDescription() const;
};

class VirtualFileSystem
{
private:
    static std::mutex mountPointsMutex_;
    static std::unordered_map<std::string, std::filesystem::path> mountPoints_;

public:
    static void SetVfsMountPoint(const std::string& mountPoint, const std::filesystem::path& path);
    static void SetStandardVfsMountPoints(const std::filesystem::path& kajiyaPath);
    static std::filesystem::path CanonicalPathFromVfs(const std::filesystem::path& path);
    static std::filesystem::path NormalizedPathFromVfs(const std::filesystem::path& path);
};

} // namespace tekki::backend