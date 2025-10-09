#include "allocator_internal.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace tekki::gpu_allocator {

// ============================================================================
// AllocationSizes Implementation
// ============================================================================

uint64_t AllocationSizes::AdjustMemblockSize(uint64_t size, const char* kind) {
    constexpr uint64_t MIN_SIZE = 4 * MB;
    constexpr uint64_t MAX_SIZE = 256 * MB;

    size = std::clamp(size, MIN_SIZE, MAX_SIZE);

    if (size % (4 * MB) == 0) {
        return size;
    }

    const uint64_t val = size / (4 * MB) + 1;
    const uint64_t new_size = val * 4 * MB;
    spdlog::warn("{} memory block size must be a multiple of 4MB, clamping to {}MB",
                 kind, new_size / MB);

    return new_size;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string FmtBytes(uint64_t amount) {
    constexpr const char* SUFFIX[] = {"B", "KB", "MB", "GB", "TB"};

    size_t idx = 0;
    double print_amount = static_cast<double>(amount);

    while (amount >= 1024 && idx < 4) {
        print_amount = static_cast<double>(amount) / 1024.0;
        amount /= 1024;
        idx++;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", print_amount, SUFFIX[idx]);
    return std::string(buf);
}

// ============================================================================
// DedicatedBlockAllocator Implementation
// ============================================================================

DedicatedBlockAllocator::DedicatedBlockAllocator(uint64_t size)
    : size_(size) {}

Result<std::pair<uint64_t, uint64_t>> DedicatedBlockAllocator::Allocate(
    uint64_t size,
    uint64_t /*alignment*/,
    AllocationType /*allocation_type*/,
    uint64_t /*granularity*/,
    const std::string& name)
{
    if (allocated_ != 0) {
        return AllocationError::OutOfMemory();
    }

    if (size_ != size) {
        return AllocationError::Internal("DedicatedBlockAllocator size must match allocation size.");
    }

    allocated_ = size;
    name_ = name;

    constexpr uint64_t DUMMY_ID = 1;
    return std::make_pair(0ull, DUMMY_ID);
}

ResultVoid DedicatedBlockAllocator::Free(std::optional<uint64_t> chunk_id) {
    if (!chunk_id.has_value() || chunk_id.value() != 1) {
        return AllocationError::Internal("Chunk ID must be 1.");
    }
    allocated_ = 0;
    return Ok();
}

ResultVoid DedicatedBlockAllocator::RenameAllocation(
    std::optional<uint64_t> chunk_id,
    const std::string& name)
{
    if (!chunk_id.has_value() || chunk_id.value() != 1) {
        return AllocationError::Internal("Chunk ID must be 1.");
    }
    name_ = name;
    return Ok();
}

void DedicatedBlockAllocator::ReportMemoryLeaks(
    size_t memory_type_index,
    size_t memory_block_index) const
{
    if (allocated_ == 0) {
        return;
    }

    const std::string& name = name_.value_or("");
    spdlog::warn(
        "leak detected: {{\n"
        "    memory type: {}\n"
        "    memory block: {}\n"
        "    dedicated allocation: {{\n"
        "        size: 0x{:x},\n"
        "        name: {}\n"
        "    }}\n"
        "}}",
        memory_type_index,
        memory_block_index,
        size_,
        name
    );
}

std::vector<gpu_allocator::AllocationReport> DedicatedBlockAllocator::ReportAllocations() const {
    if (allocated_ == 0) {
        return {};
    }

    gpu_allocator::AllocationReport report;
    report.Name = name_.value_or("<Unnamed Dedicated allocation>");
    report.Offset = 0;
    report.Size = size_;
    return {report};
}

// ============================================================================
// FreeListAllocator Implementation
// ============================================================================

FreeListAllocator::FreeListAllocator(uint64_t size)
    : size_(size)
{
    constexpr uint64_t INITIAL_CHUNK_ID = 1;

    MemoryChunk initial_chunk;
    initial_chunk.ChunkId = INITIAL_CHUNK_ID;
    initial_chunk.Size = size;
    initial_chunk.Offset = 0;
    initial_chunk.Type = AllocationType::Free;

    chunks_[INITIAL_CHUNK_ID] = initial_chunk;
    free_chunks_.insert(INITIAL_CHUNK_ID);
}

Result<uint64_t> FreeListAllocator::GetNewChunkId() {
    if (chunk_id_counter_ == UINT64_MAX) {
        return AllocationError::OutOfMemory();
    }

    const uint64_t id = chunk_id_counter_++;
    if (id == 0) {
        return AllocationError::Internal("New chunk id was 0, which is not allowed.");
    }

    return id;
}

void FreeListAllocator::RemoveIdFromFreeList(uint64_t chunk_id) {
    free_chunks_.erase(chunk_id);
}

ResultVoid FreeListAllocator::MergeFreeChunks(uint64_t chunk_left, uint64_t chunk_right) {
    auto right_it = chunks_.find(chunk_right);
    if (right_it == chunks_.end()) {
        return AllocationError::Internal("Chunk ID not present in chunk list.");
    }

    const uint64_t right_size = right_it->second.Size;
    const auto right_next = right_it->second.Next;

    RemoveIdFromFreeList(chunk_right);
    chunks_.erase(right_it);

    auto left_it = chunks_.find(chunk_left);
    if (left_it == chunks_.end()) {
        return AllocationError::Internal("Chunk ID not present in chunk list.");
    }

    left_it->second.Next = right_next;
    left_it->second.Size += right_size;

    if (right_next.has_value()) {
        auto next_it = chunks_.find(right_next.value());
        if (next_it == chunks_.end()) {
            return AllocationError::Internal("Chunk ID not present in chunk list.");
        }
        next_it->second.Prev = chunk_left;
    }

    return Ok();
}

Result<std::pair<uint64_t, uint64_t>> FreeListAllocator::Allocate(
    uint64_t size,
    uint64_t alignment,
    AllocationType allocation_type,
    uint64_t granularity,
    const std::string& name)
{
    const uint64_t free_size = size_ - allocated_;
    if (size > free_size) {
        return AllocationError::OutOfMemory();
    }

    constexpr bool USE_BEST_FIT = true;

    std::optional<uint64_t> best_fit_id;
    uint64_t best_offset = 0;
    uint64_t best_aligned_size = 0;
    uint64_t best_chunk_size = 0;

    for (uint64_t current_chunk_id : free_chunks_) {
        auto it = chunks_.find(current_chunk_id);
        if (it == chunks_.end()) {
            return AllocationError::Internal("Chunk ID in free list is not present in chunk list.");
        }

        const auto& current_chunk = it->second;

        if (current_chunk.Size < size) {
            continue;
        }

        uint64_t offset = AlignUp(current_chunk.Offset, alignment);

        if (current_chunk.Prev.has_value()) {
            auto prev_it = chunks_.find(current_chunk.Prev.value());
            if (prev_it == chunks_.end()) {
                return AllocationError::Internal("Invalid previous chunk reference.");
            }
            const auto& previous = prev_it->second;

            if (IsOnSamePage(previous.Offset, previous.Size, offset, granularity) &&
                HasGranularityConflict(previous.Type, allocation_type))
            {
                offset = AlignUp(offset, granularity);
            }
        }

        const uint64_t padding = offset - current_chunk.Offset;
        const uint64_t aligned_size = padding + size;

        if (aligned_size > current_chunk.Size) {
            continue;
        }

        if (current_chunk.Next.has_value()) {
            auto next_it = chunks_.find(current_chunk.Next.value());
            if (next_it == chunks_.end()) {
                return AllocationError::Internal("Invalid next chunk reference.");
            }
            const auto& next = next_it->second;

            if (IsOnSamePage(offset, size, next.Offset, granularity) &&
                HasGranularityConflict(allocation_type, next.Type))
            {
                continue;
            }
        }

        if (USE_BEST_FIT) {
            if (!best_fit_id.has_value() || current_chunk.Size < best_chunk_size) {
                best_fit_id = current_chunk_id;
                best_aligned_size = aligned_size;
                best_offset = offset;
                best_chunk_size = current_chunk.Size;
            }
        } else {
            best_fit_id = current_chunk_id;
            best_aligned_size = aligned_size;
            best_offset = offset;
            best_chunk_size = current_chunk.Size;
            break;
        }
    }

    if (!best_fit_id.has_value()) {
        return AllocationError::OutOfMemory();
    }

    const uint64_t first_fit_id = best_fit_id.value();
    uint64_t chunk_id;

    if (best_chunk_size > best_aligned_size) {
        auto new_chunk_id_result = GetNewChunkId();
        if (IsErr(new_chunk_id_result)) {
            return UnwrapErr(new_chunk_id_result);
        }
        const uint64_t new_chunk_id = Unwrap(new_chunk_id_result);

        auto free_chunk_it = chunks_.find(first_fit_id);
        if (free_chunk_it == chunks_.end()) {
            return AllocationError::Internal("Chunk ID must be in chunk list.");
        }

        MemoryChunk new_chunk;
        new_chunk.ChunkId = new_chunk_id;
        new_chunk.Size = best_aligned_size;
        new_chunk.Offset = free_chunk_it->second.Offset;
        new_chunk.Type = allocation_type;
        new_chunk.Name = name;
        new_chunk.Prev = free_chunk_it->second.Prev;
        new_chunk.Next = first_fit_id;

        free_chunk_it->second.Prev = new_chunk_id;
        free_chunk_it->second.Offset += best_aligned_size;
        free_chunk_it->second.Size -= best_aligned_size;

        if (new_chunk.Prev.has_value()) {
            auto prev_it = chunks_.find(new_chunk.Prev.value());
            if (prev_it == chunks_.end()) {
                return AllocationError::Internal("Invalid previous chunk reference.");
            }
            prev_it->second.Next = new_chunk_id;
        }

        chunks_[new_chunk_id] = new_chunk;
        chunk_id = new_chunk_id;
    } else {
        auto chunk_it = chunks_.find(first_fit_id);
        if (chunk_it == chunks_.end()) {
            return AllocationError::Internal("Invalid chunk reference.");
        }

        chunk_it->second.Type = allocation_type;
        chunk_it->second.Name = name;

        RemoveIdFromFreeList(first_fit_id);
        chunk_id = first_fit_id;
    }

    allocated_ += best_aligned_size;

    return std::make_pair(best_offset, chunk_id);
}

ResultVoid FreeListAllocator::Free(std::optional<uint64_t> chunk_id) {
    if (!chunk_id.has_value()) {
        return AllocationError::Internal("Chunk ID must be a valid value.");
    }

    const uint64_t id = chunk_id.value();
    auto it = chunks_.find(id);
    if (it == chunks_.end()) {
        return AllocationError::Internal("Attempting to free chunk that is not in chunk list.");
    }

    auto& chunk = it->second;
    chunk.Type = AllocationType::Free;
    chunk.Name = std::nullopt;

    allocated_ -= chunk.Size;
    free_chunks_.insert(id);

    const auto next_id = chunk.Next;
    const auto prev_id = chunk.Prev;

    if (next_id.has_value()) {
        auto next_it = chunks_.find(next_id.value());
        if (next_it != chunks_.end() && next_it->second.Type == AllocationType::Free) {
            auto result = MergeFreeChunks(id, next_id.value());
            if (IsErr(result)) {
                return UnwrapErr(result);
            }
        }
    }

    if (prev_id.has_value()) {
        auto prev_it = chunks_.find(prev_id.value());
        if (prev_it != chunks_.end() && prev_it->second.Type == AllocationType::Free) {
            auto result = MergeFreeChunks(prev_id.value(), id);
            if (IsErr(result)) {
                return UnwrapErr(result);
            }
        }
    }

    return Ok();
}

ResultVoid FreeListAllocator::RenameAllocation(
    std::optional<uint64_t> chunk_id,
    const std::string& name)
{
    if (!chunk_id.has_value()) {
        return AllocationError::Internal("Chunk ID must be a valid value.");
    }

    auto it = chunks_.find(chunk_id.value());
    if (it == chunks_.end()) {
        return AllocationError::Internal("Attempting to rename chunk that is not in chunk list.");
    }

    if (it->second.Type == AllocationType::Free) {
        return AllocationError::Internal("Attempting to rename a freed allocation.");
    }

    it->second.Name = name;
    return Ok();
}

void FreeListAllocator::ReportMemoryLeaks(
    size_t memory_type_index,
    size_t memory_block_index) const
{
    for (const auto& [chunk_id, chunk] : chunks_) {
        if (chunk.Type == AllocationType::Free) {
            continue;
        }

        const std::string& name = chunk.Name.value_or("");
        const char* type_str = chunk.Type == AllocationType::Linear ? "Linear" : "Non-Linear";

        spdlog::warn(
            "leak detected: {{\n"
            "    memory type: {}\n"
            "    memory block: {}\n"
            "    chunk: {{\n"
            "        chunk_id: {},\n"
            "        size: 0x{:x},\n"
            "        offset: 0x{:x},\n"
            "        allocation_type: {},\n"
            "        name: {}\n"
            "    }}\n"
            "}}",
            memory_type_index,
            memory_block_index,
            chunk_id,
            chunk.Size,
            chunk.Offset,
            type_str,
            name
        );
    }
}

std::vector<gpu_allocator::AllocationReport> FreeListAllocator::ReportAllocations() const {
    std::vector<gpu_allocator::AllocationReport> reports;

    for (const auto& [_, chunk] : chunks_) {
        if (chunk.Type == AllocationType::Free) {
            continue;
        }

        gpu_allocator::AllocationReport report;
        report.Name = chunk.Name.value_or("<Unnamed FreeList allocation>");
        report.Offset = chunk.Offset;
        report.Size = chunk.Size;
        reports.push_back(report);
    }

    return reports;
}

} // namespace tekki::gpu_allocator
