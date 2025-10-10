#pragma once

#include "tekki/gpu_allocator/allocator.h"

namespace tekki {

// 专用块分配器 - 用于专用分配(dedicated allocations)
class DedicatedBlockAllocator : public SubAllocator {
public:
    explicit DedicatedBlockAllocator(uint64_t size);
    ~DedicatedBlockAllocator() override = default;

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

    bool SupportsGeneralAllocations() const override { return false; }

    uint64_t Allocated() const override { return allocated_; }

private:
    uint64_t size_;
    uint64_t allocated_;
    std::string name_;
};

} // namespace tekki
