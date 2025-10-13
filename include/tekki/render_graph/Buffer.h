#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <memory>
#include <vulkan/vulkan.h>

namespace tekki::render_graph {

// Forward declarations
namespace AnyRenderResource {
    struct OwnedImage;
    struct ImportedImage;
    struct OwnedBuffer;
    struct ImportedBuffer;
    struct ImportedRayTracingAcceleration;
}
struct PendingRenderResourceInfo;

// Memory location enum (matching gpu-allocator)
enum class MemoryLocation {
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
    Unknown,
};

// Buffer descriptor matching Rust version
struct BufferDesc {
    using Resource = Buffer;  // Type alias for template resolution

    size_t size = 0;
    VkBufferUsageFlags usage = 0;
    MemoryLocation memory_location = MemoryLocation::GpuOnly;
    std::optional<uint64_t> alignment = std::nullopt;

    static BufferDesc NewGpuOnly(size_t sz, VkBufferUsageFlags usg) {
        BufferDesc desc;
        desc.size = sz;
        desc.usage = usg;
        desc.memory_location = MemoryLocation::GpuOnly;
        return desc;
    }
};

// Buffer resource
class Buffer {
public:
    using Desc = BufferDesc;

    VkBuffer raw = VK_NULL_HANDLE;
    VkBuffer Raw = VK_NULL_HANDLE;  // Alias for compatibility
    BufferDesc desc;

    Buffer() = default;
    explicit Buffer(const BufferDesc& d) : desc(d) {}

    const BufferDesc& GetDesc() const { return desc; }

    // Helper to borrow resource from variant
    template<typename VariantType>
    static const Buffer* BorrowResource(const VariantType& variant) {
        if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(variant)) {
            return std::get<AnyRenderResource::OwnedBuffer>(variant).resource.get();
        } else if (std::holds_alternative<AnyRenderResource::ImportedBuffer>(variant)) {
            return std::get<AnyRenderResource::ImportedBuffer>(variant).resource.get();
        }
        return nullptr;
    }
};

} // namespace tekki::render_graph
