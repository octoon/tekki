#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <memory>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/buffer.h"

namespace tekki::render_graph {

// Re-export Buffer types from backend (like Rust does)
using Buffer = backend::vulkan::Buffer;
using BufferDesc = backend::vulkan::BufferDesc;

// Forward declarations for resource variant types
namespace AnyRenderResource {
    struct OwnedImage;
    struct ImportedImage;
    struct OwnedBuffer;
    struct ImportedBuffer;
    struct ImportedRayTracingAcceleration;
}
struct PendingRenderResourceInfo;

} // namespace tekki::render_graph
