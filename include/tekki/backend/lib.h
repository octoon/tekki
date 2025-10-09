#pragma once

#include <vector>
#include <memory>
#include <string>
#include "tekki/core/result.h"
#include <glm/glm.hpp>

namespace tekki::backend {

// Forward declarations
class Bytes;
class ChunkyList;
class DynamicConstants;
class File;
class PipelineCache;
class RustShaderCompiler;
class ShaderCompiler;
class TransientResourceCache;
class Vulkan;

// External dependencies
class Ash;
class GpuAllocator;
class GpuProfiler;
class RspirvReflect;
class VkSync;

class Device;
class Image;
class RenderBackend;

constexpr uint32_t MAX_DESCRIPTOR_SETS = 4;

class BackendError : public std::exception {
public:
    BackendError(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
    
private:
    std::string message_;
};

std::string CanonicalPathFromVfs(const std::string& path);
std::string NormalizedPathFromVfs(const std::string& path);
void SetVfsMountPoint(const std::string& mountPoint);

} // namespace tekki::backend