#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <vulkan/vulkan.h>
#include <tekki/gpu_allocator/vulkan/allocator.h>

namespace tekki::backend::vulkan {

// Forward declarations
class Device;
struct Buffer;

struct BufferDesc {
    using Resource = Buffer;  // Type alias for template resolution

    // Use lowercase names to match usage
    size_t size;
    VkBufferUsageFlags usage;
    tekki::MemoryLocation memory_location;
    std::optional<uint64_t> alignment;

    BufferDesc() = default;

    BufferDesc(size_t size_param, VkBufferUsageFlags usage_param, tekki::MemoryLocation memory_location_param, std::optional<uint64_t> alignment_param = std::nullopt)
        : size(size_param), usage(usage_param), memory_location(memory_location_param), alignment(alignment_param) {}

    static BufferDesc NewGpuOnly(size_t size_param, VkBufferUsageFlags usage_param) {
        return BufferDesc(size_param, usage_param, tekki::MemoryLocation::GpuOnly, std::nullopt);
    }

    static BufferDesc NewCpuToGpu(size_t size_param, VkBufferUsageFlags usage_param) {
        return BufferDesc(size_param, usage_param, tekki::MemoryLocation::CpuToGpu, std::nullopt);
    }

    static BufferDesc NewGpuToCpu(size_t size_param, VkBufferUsageFlags usage_param) {
        return BufferDesc(size_param, usage_param, tekki::MemoryLocation::GpuToCpu, std::nullopt);
    }

    BufferDesc WithAlignment(uint64_t alignment_param) {
        alignment = alignment_param;
        return *this;
    }

    bool operator==(const BufferDesc& other) const {
        return size == other.size &&
               usage == other.usage &&
               memory_location == other.memory_location &&
               alignment == other.alignment;
    }
};

struct Buffer {
    using Desc = BufferDesc;  // Type alias for Handle<Buffer> template

    VkBuffer Raw;
    BufferDesc desc;
    tekki::Allocation Allocation;

    // Default constructor
    Buffer() : Raw(VK_NULL_HANDLE), desc(), Allocation() {}

    // Constructor
    Buffer(VkBuffer raw, const BufferDesc& d, tekki::Allocation allocation)
        : Raw(raw), desc(d), Allocation(std::move(allocation)) {}

    // Move constructor
    Buffer(Buffer&& other) noexcept
        : Raw(other.Raw), desc(other.desc), Allocation(std::move(other.Allocation)) {
        other.Raw = VK_NULL_HANDLE;
    }

    // Move assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            Raw = other.Raw;
            desc = other.desc;
            Allocation = std::move(other.Allocation);
            other.Raw = VK_NULL_HANDLE;
        }
        return *this;
    }

    // Delete copy operations (buffers should not be copied)
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    uint64_t DeviceAddress(VkDevice device) const {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = Raw;
        return vkGetBufferDeviceAddress(device, &addressInfo);
    }

    // Get mapped memory slice (for CPU-accessible buffers)
    std::vector<uint8_t> GetMappedSlice() const {
        const uint8_t* ptr = Allocation.MappedSlice();
        if (ptr) {
            return std::vector<uint8_t>(ptr, ptr + desc.size);
        }
        return std::vector<uint8_t>();
    }

    // Get mutable mapped memory pointer
    uint8_t* GetMappedPtr() {
        return Allocation.MappedSlice();
    }

    const uint8_t* GetMappedPtr() const {
        return Allocation.MappedSlice();
    }
};

} // namespace tekki::backend::vulkan

// Hash specialization for BufferDesc
namespace std {
    template<>
    struct hash<tekki::backend::vulkan::BufferDesc> {
        size_t operator()(const tekki::backend::vulkan::BufferDesc& desc) const {
            size_t h1 = std::hash<size_t>{}(desc.size);
            size_t h2 = std::hash<VkBufferUsageFlags>{}(desc.usage);
            size_t h3 = std::hash<int>{}(static_cast<int>(desc.memory_location));
            size_t h4 = desc.alignment.has_value() ? std::hash<uint64_t>{}(desc.alignment.value()) : 0;
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}
