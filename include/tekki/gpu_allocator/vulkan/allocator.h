#pragma once

#include "tekki/gpu_allocator/allocator.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <optional>

namespace tekki {

// Vulkan 分配方案
enum class AllocationScheme {
    DedicatedBuffer,        // 专用 buffer 分配
    DedicatedImage,         // 专用 image 分配
    GpuAllocatorManaged,    // GPU 分配器管理的分配
};

// Vulkan 分配创建描述符
struct AllocationCreateDesc {
    const char* name;                       // 分配名称
    VkMemoryRequirements requirements;      // Vulkan 内存需求
    MemoryLocation location;                // 内存位置
    bool linear;                            // 是否是线性资源
    AllocationScheme allocation_scheme;     // 分配方案
    VkBuffer dedicated_buffer = VK_NULL_HANDLE;  // 专用 buffer (如果使用 DedicatedBuffer)
    VkImage dedicated_image = VK_NULL_HANDLE;    // 专用 image (如果使用 DedicatedImage)
};

// Vulkan 分配
class Allocation {
public:
    Allocation() = default;
    ~Allocation() = default;

    // 禁止拷贝
    Allocation(const Allocation&) = delete;
    Allocation& operator=(const Allocation&) = delete;

    // 允许移动
    Allocation(Allocation&&) noexcept;
    Allocation& operator=(Allocation&&) noexcept;

    // 获取 chunk ID
    std::optional<uint64_t> ChunkId() const;

    // 获取内存属性标志
    VkMemoryPropertyFlags MemoryProperties() const;

    // 获取底层 DeviceMemory (unsafe)
    VkDeviceMemory Memory() const;

    // 是否是专用分配
    bool IsDedicated() const;

    // 获取偏移量
    uint64_t Offset() const;

    // 获取大小
    uint64_t Size() const;

    // 获取映射指针
    void* MappedPtr() const;

    // 获取映射的切片
    uint8_t* MappedSlice();
    const uint8_t* MappedSlice() const;

    // 是否为空
    bool IsNull() const;

private:
    friend class Allocator;

    std::optional<uint64_t> chunk_id_;
    uint64_t offset_ = 0;
    uint64_t size_ = 0;
    size_t memory_block_index_ = static_cast<size_t>(-1);
    size_t memory_type_index_ = static_cast<size_t>(-1);
    VkDeviceMemory device_memory_ = VK_NULL_HANDLE;
    void* mapped_ptr_ = nullptr;
    bool dedicated_allocation_ = false;
    VkMemoryPropertyFlags memory_properties_ = {};
    std::string name_;
};

// Vulkan 分配器创建描述符
struct AllocatorCreateDesc {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical_device;
    AllocatorDebugSettings debug_settings;
    bool buffer_device_address;
    AllocationSizes allocation_sizes;
};

// 内存块
struct MemoryBlock {
    VkDeviceMemory device_memory;
    uint64_t size;
    void* mapped_ptr;
    std::unique_ptr<SubAllocator> sub_allocator;
};

// 内存类型
struct MemoryType {
    std::vector<std::optional<MemoryBlock>> memory_blocks;
    VkMemoryPropertyFlags memory_properties;
    size_t memory_type_index;
    size_t heap_index;
    bool mappable;
    size_t active_general_blocks;
    bool buffer_device_address;

    Allocation Allocate(
        VkDevice device,
        const AllocationCreateDesc& desc,
        uint64_t granularity,
        const AllocationSizes& allocation_sizes);

    void Free(const Allocation& allocation, VkDevice device);
};

// Vulkan 分配器
class Allocator {
public:
    explicit Allocator(const AllocatorCreateDesc& desc);
    ~Allocator();

    // 禁止拷贝
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    // 禁止移动
    Allocator(Allocator&&) = delete;
    Allocator& operator=(Allocator&&) = delete;

    // 分配内存
    Allocation Allocate(const AllocationCreateDesc& desc);

    // 释放内存
    void Free(Allocation&& allocation);

    // 重命名分配
    void RenameAllocation(Allocation& allocation, const std::string& name);

    // 报告内存泄漏
    void ReportMemoryLeaks(int log_level) const;

    // 生成报告
    AllocatorReport GenerateReport() const;

    // 获取当前容量
    uint64_t Capacity() const;

private:
    std::optional<uint32_t> FindMemoryTypeIndex(
        const VkMemoryRequirements& memory_req,
        VkMemoryPropertyFlags flags) const;

    std::vector<MemoryType> memory_types_;
    std::vector<VkMemoryHeap> memory_heaps_;
    VkDevice device_;
    uint64_t buffer_image_granularity_;
    AllocatorDebugSettings debug_settings_;
    AllocationSizes allocation_sizes_;
};

} // namespace tekki
