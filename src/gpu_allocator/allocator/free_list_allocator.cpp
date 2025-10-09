#include "tekki/gpu_allocator/allocator/free_list_allocator.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace tekki {

static constexpr bool USE_BEST_FIT = true;

// 辅助函数实现
uint64_t AlignDown(uint64_t val, uint64_t alignment) {
    return val & ~(alignment - 1);
}

uint64_t AlignUp(uint64_t val, uint64_t alignment) {
    return AlignDown(val + alignment - 1, alignment);
}

bool IsOnSamePage(uint64_t offset_a, uint64_t size_a, uint64_t offset_b, uint64_t page_size) {
    uint64_t end_a = offset_a + size_a - 1;
    uint64_t end_page_a = AlignDown(end_a, page_size);
    uint64_t start_page_b = AlignDown(offset_b, page_size);

    return end_page_a == start_page_b;
}

bool HasGranularityConflict(AllocationType type0, AllocationType type1) {
    if (type0 == AllocationType::Free || type1 == AllocationType::Free) {
        return false;
    }
    return type0 != type1;
}

// FreeListAllocator 实现
FreeListAllocator::FreeListAllocator(uint64_t size)
    : size_(size),
      allocated_(0),
      chunk_id_counter_(2) { // 0 不允许, 1 被初始块使用

    // 创建初始的自由块
    MemoryChunk initial_chunk{};
    initial_chunk.chunk_id = 1;
    initial_chunk.size = size;
    initial_chunk.offset = 0;
    initial_chunk.allocation_type = AllocationType::Free;
    initial_chunk.next = std::nullopt;
    initial_chunk.prev = std::nullopt;

    chunks_[1] = initial_chunk;
    free_chunks_.insert(1);
}

uint64_t FreeListAllocator::GetNewChunkId() {
    if (chunk_id_counter_ == UINT64_MAX) {
        throw AllocationError(AllocationErrorCode::OutOfMemory);
    }

    uint64_t id = chunk_id_counter_++;
    if (id == 0) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "New chunk id was 0, which is not allowed."
        );
    }

    return id;
}

void FreeListAllocator::RemoveIdFromFreeList(uint64_t chunk_id) {
    free_chunks_.erase(chunk_id);
}

void FreeListAllocator::MergeFreeChunks(uint64_t chunk_left, uint64_t chunk_right) {
    // 从右块收集数据并移除它
    auto right_it = chunks_.find(chunk_right);
    if (right_it == chunks_.end()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Chunk ID not present in chunk list."
        );
    }

    uint64_t right_size = right_it->second.size;
    auto right_next = right_it->second.next;

    RemoveIdFromFreeList(chunk_right);
    chunks_.erase(right_it);

    // 合并到左块
    auto left_it = chunks_.find(chunk_left);
    if (left_it == chunks_.end()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Chunk ID not present in chunk list."
        );
    }

    left_it->second.next = right_next;
    left_it->second.size += right_size;

    // 修补指针
    if (right_next.has_value()) {
        auto next_it = chunks_.find(right_next.value());
        if (next_it == chunks_.end()) {
            throw AllocationError(
                AllocationErrorCode::Internal,
                "Chunk ID not present in chunk list."
            );
        }
        next_it->second.prev = chunk_left;
    }
}

std::pair<uint64_t, uint64_t> FreeListAllocator::Allocate(
    uint64_t size,
    uint64_t alignment,
    AllocationType allocation_type,
    uint64_t granularity,
    const std::string& name) {

    uint64_t free_size = size_ - allocated_;
    if (size > free_size) {
        throw AllocationError(AllocationErrorCode::OutOfMemory);
    }

    std::optional<uint64_t> best_fit_id;
    uint64_t best_offset = 0;
    uint64_t best_aligned_size = 0;
    uint64_t best_chunk_size = 0;

    // 遍历所有自由块寻找最佳匹配
    for (uint64_t current_chunk_id : free_chunks_) {
        auto it = chunks_.find(current_chunk_id);
        if (it == chunks_.end()) {
            throw AllocationError(
                AllocationErrorCode::Internal,
                "Chunk ID in free list is not present in chunk list."
            );
        }

        const MemoryChunk& current_chunk = it->second;

        if (current_chunk.size < size) {
            continue;
        }

        uint64_t offset = AlignUp(current_chunk.offset, alignment);

        // 检查与前一个块的粒度冲突
        if (current_chunk.prev.has_value()) {
            auto prev_it = chunks_.find(current_chunk.prev.value());
            if (prev_it == chunks_.end()) {
                throw AllocationError(
                    AllocationErrorCode::Internal,
                    "Invalid previous chunk reference."
                );
            }

            const MemoryChunk& previous = prev_it->second;
            if (IsOnSamePage(previous.offset, previous.size, offset, granularity) &&
                HasGranularityConflict(previous.allocation_type, allocation_type)) {
                offset = AlignUp(offset, granularity);
            }
        }

        uint64_t padding = offset - current_chunk.offset;
        uint64_t aligned_size = padding + size;

        if (aligned_size > current_chunk.size) {
            continue;
        }

        // 检查与下一个块的粒度冲突
        if (current_chunk.next.has_value()) {
            auto next_it = chunks_.find(current_chunk.next.value());
            if (next_it == chunks_.end()) {
                throw AllocationError(
                    AllocationErrorCode::Internal,
                    "Invalid next chunk reference."
                );
            }

            const MemoryChunk& next = next_it->second;
            if (IsOnSamePage(offset, size, next.offset, granularity) &&
                HasGranularityConflict(allocation_type, next.allocation_type)) {
                continue;
            }
        }

        // 使用最佳匹配或首次匹配策略
        if (USE_BEST_FIT) {
            if (!best_fit_id.has_value() || current_chunk.size < best_chunk_size) {
                best_fit_id = current_chunk_id;
                best_aligned_size = aligned_size;
                best_offset = offset;
                best_chunk_size = current_chunk.size;
            }
        } else {
            best_fit_id = current_chunk_id;
            best_aligned_size = aligned_size;
            best_offset = offset;
            best_chunk_size = current_chunk.size;
            break;
        }
    }

    if (!best_fit_id.has_value()) {
        throw AllocationError(AllocationErrorCode::OutOfMemory);
    }

    uint64_t first_fit_id = best_fit_id.value();
    uint64_t chunk_id;

    // 如果块大小大于需要的大小,分割块
    if (best_chunk_size > best_aligned_size) {
        uint64_t new_chunk_id = GetNewChunkId();

        auto free_chunk_it = chunks_.find(first_fit_id);
        if (free_chunk_it == chunks_.end()) {
            throw AllocationError(
                AllocationErrorCode::Internal,
                "Chunk ID must be in chunk list."
            );
        }

        MemoryChunk& free_chunk = free_chunk_it->second;

        // 创建新的分配块
        MemoryChunk new_chunk{};
        new_chunk.chunk_id = new_chunk_id;
        new_chunk.size = best_aligned_size;
        new_chunk.offset = free_chunk.offset;
        new_chunk.allocation_type = allocation_type;
        new_chunk.name = name;
        new_chunk.prev = free_chunk.prev;
        new_chunk.next = first_fit_id;

        // 更新自由块
        free_chunk.prev = new_chunk_id;
        free_chunk.offset += best_aligned_size;
        free_chunk.size -= best_aligned_size;

        // 更新前一个块的指针
        if (new_chunk.prev.has_value()) {
            auto prev_it = chunks_.find(new_chunk.prev.value());
            if (prev_it == chunks_.end()) {
                throw AllocationError(
                    AllocationErrorCode::Internal,
                    "Invalid previous chunk reference."
                );
            }
            prev_it->second.next = new_chunk_id;
        }

        chunks_[new_chunk_id] = new_chunk;
        chunk_id = new_chunk_id;
    } else {
        // 使用整个块
        auto chunk_it = chunks_.find(first_fit_id);
        if (chunk_it == chunks_.end()) {
            throw AllocationError(
                AllocationErrorCode::Internal,
                "Invalid chunk reference."
            );
        }

        chunk_it->second.allocation_type = allocation_type;
        chunk_it->second.name = name;

        RemoveIdFromFreeList(first_fit_id);
        chunk_id = first_fit_id;
    }

    allocated_ += best_aligned_size;

    return {best_offset, chunk_id};
}

void FreeListAllocator::Free(std::optional<uint64_t> chunk_id) {
    if (!chunk_id.has_value()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Chunk ID must be a valid value."
        );
    }

    auto it = chunks_.find(chunk_id.value());
    if (it == chunks_.end()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Attempting to free chunk that is not in chunk list."
        );
    }

    MemoryChunk& chunk = it->second;
    chunk.allocation_type = AllocationType::Free;
    chunk.name.clear();

    allocated_ -= chunk.size;

    free_chunks_.insert(chunk.chunk_id);

    auto next_id = chunk.next;
    auto prev_id = chunk.prev;

    // 尝试与下一个块合并
    if (next_id.has_value()) {
        auto next_it = chunks_.find(next_id.value());
        if (next_it != chunks_.end() &&
            next_it->second.allocation_type == AllocationType::Free) {
            MergeFreeChunks(chunk_id.value(), next_id.value());
        }
    }

    // 尝试与前一个块合并
    if (prev_id.has_value()) {
        auto prev_it = chunks_.find(prev_id.value());
        if (prev_it != chunks_.end() &&
            prev_it->second.allocation_type == AllocationType::Free) {
            MergeFreeChunks(prev_id.value(), chunk_id.value());
        }
    }
}

void FreeListAllocator::RenameAllocation(
    std::optional<uint64_t> chunk_id,
    const std::string& name) {

    if (!chunk_id.has_value()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Chunk ID must be a valid value."
        );
    }

    auto it = chunks_.find(chunk_id.value());
    if (it == chunks_.end()) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Attempting to rename chunk that is not in chunk list."
        );
    }

    if (it->second.allocation_type == AllocationType::Free) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Attempting to rename a freed allocation."
        );
    }

    it->second.name = name;
}

void FreeListAllocator::ReportMemoryLeaks(
    int log_level,
    size_t memory_type_index,
    size_t memory_block_index) const {

    for (const auto& [chunk_id, chunk] : chunks_) {
        if (chunk.allocation_type == AllocationType::Free) {
            continue;
        }

        std::string name_str = chunk.name.empty() ? "" : chunk.name;
        const char* type_str = chunk.allocation_type == AllocationType::Linear
            ? "Linear" : "Non-Linear";

        spdlog::log(
            static_cast<spdlog::level::level_enum>(log_level),
            R"(leak detected: {{
    memory type: {}
    memory block: {}
    chunk: {{
        chunk_id: {},
        size: 0x{:x},
        offset: 0x{:x},
        allocation_type: {},
        name: {}
    }}
}})",
            memory_type_index,
            memory_block_index,
            chunk_id,
            chunk.size,
            chunk.offset,
            type_str,
            name_str
        );
    }
}

std::vector<AllocationReport> FreeListAllocator::ReportAllocations() const {
    std::vector<AllocationReport> result;

    for (const auto& [chunk_id, chunk] : chunks_) {
        if (chunk.allocation_type != AllocationType::Free) {
            result.push_back(AllocationReport{
                chunk.name.empty() ? "<Unnamed FreeList allocation>" : chunk.name,
                chunk.offset,
                chunk.size
            });
        }
    }

    return result;
}

} // namespace tekki
