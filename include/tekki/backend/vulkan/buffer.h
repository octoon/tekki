#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <vulkan/vulkan.h>
#include <tekki/gpu_allocator/vulkan/allocator.h>

namespace tekki::backend::vulkan {

// Forward declaration
class Device;

struct BufferDesc {
    size_t Size;
    VkBufferUsageFlags Usage;
    tekki::MemoryLocation MemoryLocation;
    std::optional<uint64_t> Alignment;

    BufferDesc() = default;

    BufferDesc(size_t size, VkBufferUsageFlags usage, tekki::MemoryLocation memoryLocation, std::optional<uint64_t> alignment = std::nullopt)
        : Size(size), Usage(usage), MemoryLocation(memoryLocation), Alignment(alignment) {}

    static BufferDesc NewGpuOnly(size_t size, VkBufferUsageFlags usage) {
        return BufferDesc(size, usage, tekki::MemoryLocation::GpuOnly, std::nullopt);
    }

    static BufferDesc NewCpuToGpu(size_t size, VkBufferUsageFlags usage) {
        return BufferDesc(size, usage, tekki::MemoryLocation::CpuToGpu, std::nullopt);
    }

    static BufferDesc NewGpuToCpu(size_t size, VkBufferUsageFlags usage) {
        return BufferDesc(size, usage, tekki::MemoryLocation::GpuToCpu, std::nullopt);
    }

    BufferDesc WithAlignment(uint64_t alignment) {
        Alignment = alignment;
        return *this;
    }

    bool operator==(const BufferDesc& other) const {
        return Size == other.Size &&
               Usage == other.Usage &&
               MemoryLocation == other.MemoryLocation &&
               Alignment == other.Alignment;
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
            return std::vector<uint8_t>(ptr, ptr + desc.Size);
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
            size_t h1 = std::hash<size_t>{}(desc.Size);
            size_t h2 = std::hash<VkBufferUsageFlags>{}(desc.Usage);
            size_t h3 = std::hash<int>{}(static_cast<int>(desc.MemoryLocation));
            size_t h4 = desc.Alignment.has_value() ? std::hash<uint64_t>{}(desc.Alignment.value()) : 0;
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}
