#pragma once

#include <cstdint>
#include <optional>
#include <vulkan/vulkan.h>

namespace tekki::render_graph {

// Memory location enum (matching gpu-allocator)
enum class MemoryLocation {
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
    Unknown,
};

// Buffer descriptor matching Rust version
struct BufferDesc {
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
    BufferDesc desc;

    Buffer() = default;
    explicit Buffer(const BufferDesc& d) : desc(d) {}

    const BufferDesc& GetDesc() const { return desc; }
};

} // namespace tekki::render_graph
