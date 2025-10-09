#pragma once

#include "tekki/gpu_allocator/result.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace tekki {

// 内存位置枚举
enum class MemoryLocation {
    Unknown,    // 未知内存位置,让驱动决定
    GpuOnly,    // GPU 专用内存,通常最快
    CpuToGpu,   // CPU 到 GPU 的上传内存
    GpuToCpu,   // GPU 到 CPU 的回读内存
};

// 调试设置
struct AllocatorDebugSettings {
    bool log_memory_information = false;  // 启动时记录内存信息
    bool log_leaks_on_shutdown = true;    // 关闭时记录内存泄漏
    bool store_stack_traces = false;      // 存储每个分配的堆栈跟踪
    bool log_allocations = false;         // 记录每次分配
    bool log_frees = false;               // 记录每次释放
    bool log_stack_traces = false;        // 记录堆栈跟踪
};

// 内存块大小配置
class AllocationSizes {
public:
    static constexpr uint64_t MB = 1024 * 1024;
    static constexpr uint64_t DEFAULT_DEVICE_MEMBLOCK_SIZE = 256 * MB;
    static constexpr uint64_t DEFAULT_HOST_MEMBLOCK_SIZE = 64 * MB;

    // 设置最小设备和主机内存块大小
    AllocationSizes(uint64_t device_memblock_size = DEFAULT_DEVICE_MEMBLOCK_SIZE,
                    uint64_t host_memblock_size = DEFAULT_HOST_MEMBLOCK_SIZE);

    // 设置最大设备内存块大小
    AllocationSizes& WithMaxDeviceMemblockSize(uint64_t size);

    // 设置最大主机内存块大小
    AllocationSizes& WithMaxHostMemblockSize(uint64_t size);

    // 获取内存块大小
    uint64_t GetMemblockSize(bool is_host, size_t count) const;

private:
    static uint64_t AdjustMemblockSize(uint64_t size, const char* kind);

    uint64_t min_device_memblock_size_;
    uint64_t max_device_memblock_size_;
    uint64_t min_host_memblock_size_;
    uint64_t max_host_memblock_size_;
};

// 分配报告
struct AllocationReport {
    std::string name;
    uint64_t offset;
    uint64_t size;
};

// 内存块报告
struct MemoryBlockReport {
    uint64_t size;
    std::ranges::iota_view<size_t, size_t> allocations;
};

// 分配器报告
struct AllocatorReport {
    std::vector<AllocationReport> allocations;
    std::vector<MemoryBlockReport> blocks;
    uint64_t total_allocated_bytes;
    uint64_t total_capacity_bytes;
};

// 内部使用的分配类型
enum class AllocationType : uint8_t {
    Free,
    Linear,
    NonLinear,
};

// SubAllocator 基类
class SubAllocator {
public:
    virtual ~SubAllocator() = default;

    // 分配内存
    virtual std::pair<uint64_t, uint64_t> Allocate(
        uint64_t size,
        uint64_t alignment,
        AllocationType allocation_type,
        uint64_t granularity,
        const std::string& name) = 0;

    // 释放内存
    virtual void Free(std::optional<uint64_t> chunk_id) = 0;

    // 重命名分配
    virtual void RenameAllocation(std::optional<uint64_t> chunk_id, const std::string& name) = 0;

    // 报告内存泄漏
    virtual void ReportMemoryLeaks(
        int log_level,
        size_t memory_type_index,
        size_t memory_block_index) const = 0;

    // 报告所有分配
    virtual std::vector<AllocationReport> ReportAllocations() const = 0;

    // 是否支持一般分配
    virtual bool SupportsGeneralAllocations() const = 0;

    // 已分配的内存大小
    virtual uint64_t Allocated() const = 0;

    // 是否为空
    bool IsEmpty() const { return Allocated() == 0; }
};

// 格式化字节数
std::string FormatBytes(uint64_t amount);

} // namespace tekki
