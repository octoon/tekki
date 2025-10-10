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
    VkBuffer Raw;
    BufferDesc Desc;
    tekki::Allocation Allocation;

    uint64_t DeviceAddress(VkDevice device) const {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = Raw;
        return vkGetBufferDeviceAddress(device, &addressInfo);
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
