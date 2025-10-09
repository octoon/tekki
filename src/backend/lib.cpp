#include "tekki/backend/lib.h"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace tekki::backend {

std::string CanonicalPathFromVfs(const std::string& path) {
    try {
        return std::filesystem::canonical(path).string();
    } catch (const std::filesystem::filesystem_error& e) {
        throw BackendError("Failed to get canonical path: " + std::string(e.what()));
    }
}

std::string NormalizedPathFromVfs(const std::string& path) {
    try {
        return std::filesystem::path(path).lexically_normal().string();
    } catch (const std::filesystem::filesystem_error& e) {
        throw BackendError("Failed to normalize path: " + std::string(e.what()));
    }
}

void SetVfsMountPoint(const std::string& mountPoint) {
    // Implementation depends on specific VFS system
    // This is a placeholder that would need to be adapted to the actual VFS implementation
    try {
        // VFS mount point setting logic would go here
        // For now, just validate the path exists
        if (!std::filesystem::exists(mountPoint)) {
            throw BackendError("Mount point does not exist: " + mountPoint);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw BackendError("Failed to set VFS mount point: " + std::string(e.what()));
    }
}

} // namespace tekki::backend