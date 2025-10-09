#include "tekki/gpu_allocator/allocator/dedicated_block_allocator.h"
#include <spdlog/spdlog.h>

namespace tekki {

DedicatedBlockAllocator::DedicatedBlockAllocator(uint64_t size)
    : size_(size), allocated_(0) {}

std::pair<uint64_t, uint64_t> DedicatedBlockAllocator::Allocate(
    uint64_t size,
    uint64_t /*alignment*/,
    AllocationType /*allocation_type*/,
    uint64_t /*granularity*/,
    const std::string& name) {

    if (allocated_ != 0) {
        throw AllocationError(AllocationErrorCode::OutOfMemory);
    }

    if (size_ != size) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "DedicatedBlockAllocator size must match allocation size."
        );
    }

    allocated_ = size;
    name_ = name;

    // Dummy chunk ID (总是 1)
    return {0, 1};
}

void DedicatedBlockAllocator::Free(std::optional<uint64_t> chunk_id) {
    if (!chunk_id.has_value() || chunk_id.value() != 1) {
        throw AllocationError(AllocationErrorCode::Internal, "Chunk ID must be 1.");
    }

    allocated_ = 0;
}

void DedicatedBlockAllocator::RenameAllocation(
    std::optional<uint64_t> chunk_id,
    const std::string& name) {

    if (!chunk_id.has_value() || chunk_id.value() != 1) {
        throw AllocationError(AllocationErrorCode::Internal, "Chunk ID must be 1.");
    }

    name_ = name;
}

void DedicatedBlockAllocator::ReportMemoryLeaks(
    int log_level,
    size_t memory_type_index,
    size_t memory_block_index) const {

    std::string name_str = name_.empty() ? "" : name_;

    spdlog::log(
        static_cast<spdlog::level::level_enum>(log_level),
        R"(leak detected: {{
    memory type: {}
    memory block: {}
    dedicated allocation: {{
        size: 0x{:x},
        name: {}
    }}
}})",
        memory_type_index,
        memory_block_index,
        size_,
        name_str
    );
}

std::vector<AllocationReport> DedicatedBlockAllocator::ReportAllocations() const {
    std::vector<AllocationReport> result;

    if (allocated_ > 0) {
        result.push_back(AllocationReport{
            name_.empty() ? "<Unnamed Dedicated allocation>" : name_,
            0,
            size_
        });
    }

    return result;
}

} // namespace tekki
