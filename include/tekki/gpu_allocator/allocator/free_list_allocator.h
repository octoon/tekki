#pragma once

#include "tekki/gpu_allocator/allocator.h"
#include <unordered_map>
#include <unordered_set>

namespace tekki {

// 内存块信息
struct MemoryChunk {
    uint64_t chunk_id;
    uint64_t size;
    uint64_t offset;
    AllocationType allocation_type;
    std::string name;
    std::optional<uint64_t> next;
    std::optional<uint64_t> prev;
};

// 自由列表分配器 - 用于子分配(sub-allocations)
class FreeListAllocator : public SubAllocator {
public:
    explicit FreeListAllocator(uint64_t size);
    ~FreeListAllocator() override = default;

    std::pair<uint64_t, uint64_t> Allocate(
        uint64_t size,
        uint64_t alignment,
        AllocationType allocation_type,
        uint64_t granularity,
        const std::string& name) override;

    void Free(std::optional<uint64_t> chunk_id) override;

    void RenameAllocation(std::optional<uint64_t> chunk_id, const std::string& name) override;

    void ReportMemoryLeaks(
        int log_level,
        size_t memory_type_index,
        size_t memory_block_index) const override;

    std::vector<AllocationReport> ReportAllocations() const override;

    bool SupportsGeneralAllocations() const override { return true; }

    uint64_t Allocated() const override { return allocated_; }

private:
    uint64_t GetNewChunkId();
    void RemoveIdFromFreeList(uint64_t chunk_id);
    void MergeFreeChunks(uint64_t chunk_left, uint64_t chunk_right);

    uint64_t size_;
    uint64_t allocated_;
    uint64_t chunk_id_counter_;
    std::unordered_map<uint64_t, MemoryChunk> chunks_;
    std::unordered_set<uint64_t> free_chunks_;
};

// 辅助函数
uint64_t AlignDown(uint64_t val, uint64_t alignment);
uint64_t AlignUp(uint64_t val, uint64_t alignment);
bool IsOnSamePage(uint64_t offset_a, uint64_t size_a, uint64_t offset_b, uint64_t page_size);
bool HasGranularityConflict(AllocationType type0, AllocationType type1);

} // namespace tekki
