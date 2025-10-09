#pragma once

#include <tekki/gpu_allocator/gpu_allocator.h>

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tekki::gpu_allocator {

// ============================================================================
// Internal Types (forward declared in public header)
// ============================================================================

enum class AllocationType : uint8_t {
    Free,
    Linear,
    NonLinear,
};

// ============================================================================
// SubAllocator Interface
// ============================================================================

class SubAllocator {
public:
    virtual ~SubAllocator() = default;

    /// Allocate a sub-region
    virtual Result<std::pair<uint64_t, uint64_t>> Allocate(
        uint64_t size,
        uint64_t alignment,
        AllocationType allocation_type,
        uint64_t granularity,
        const std::string& name
    ) = 0;

    /// Free a sub-allocation
    virtual ResultVoid Free(std::optional<uint64_t> chunk_id) = 0;

    /// Rename an allocation
    virtual ResultVoid RenameAllocation(std::optional<uint64_t> chunk_id, const std::string& name) = 0;

    /// Report memory leaks
    virtual void ReportMemoryLeaks(size_t memory_type_index, size_t memory_block_index) const = 0;

    /// Report all allocations
    virtual std::vector<gpu_allocator::AllocationReport> ReportAllocations() const = 0;

    /// Returns true if this allocator supports general allocations (not dedicated)
    virtual bool SupportsGeneralAllocations() const = 0;

    /// Returns the amount of allocated memory
    virtual uint64_t Allocated() const = 0;

    /// Returns true if no allocations exist
    bool IsEmpty() const { return Allocated() == 0; }
};

// ============================================================================
// DedicatedBlockAllocator
// ============================================================================

class DedicatedBlockAllocator : public SubAllocator {
public:
    explicit DedicatedBlockAllocator(uint64_t size);

    Result<std::pair<uint64_t, uint64_t>> Allocate(
        uint64_t size,
        uint64_t alignment,
        AllocationType allocation_type,
        uint64_t granularity,
        const std::string& name
    ) override;

    ResultVoid Free(std::optional<uint64_t> chunk_id) override;
    ResultVoid RenameAllocation(std::optional<uint64_t> chunk_id, const std::string& name) override;
    void ReportMemoryLeaks(size_t memory_type_index, size_t memory_block_index) const override;
    std::vector<gpu_allocator::AllocationReport> ReportAllocations() const override;
    bool SupportsGeneralAllocations() const override { return false; }
    uint64_t Allocated() const override { return allocated_; }

private:
    uint64_t size_;
    uint64_t allocated_ = 0;
    std::optional<std::string> name_;
};

// ============================================================================
// FreeListAllocator
// ============================================================================

struct MemoryChunk {
    uint64_t ChunkId = 0;
    uint64_t Size = 0;
    uint64_t Offset = 0;
    AllocationType Type = AllocationType::Free;
    std::optional<std::string> Name;
    std::optional<uint64_t> Next;
    std::optional<uint64_t> Prev;
};

class FreeListAllocator : public SubAllocator {
public:
    explicit FreeListAllocator(uint64_t size);

    Result<std::pair<uint64_t, uint64_t>> Allocate(
        uint64_t size,
        uint64_t alignment,
        AllocationType allocation_type,
        uint64_t granularity,
        const std::string& name
    ) override;

    ResultVoid Free(std::optional<uint64_t> chunk_id) override;
    ResultVoid RenameAllocation(std::optional<uint64_t> chunk_id, const std::string& name) override;
    void ReportMemoryLeaks(size_t memory_type_index, size_t memory_block_index) const override;
    std::vector<gpu_allocator::AllocationReport> ReportAllocations() const override;
    bool SupportsGeneralAllocations() const override { return true; }
    uint64_t Allocated() const override { return allocated_; }

private:
    Result<uint64_t> GetNewChunkId();
    void RemoveIdFromFreeList(uint64_t chunk_id);
    ResultVoid MergeFreeChunks(uint64_t chunk_left, uint64_t chunk_right);

    uint64_t size_;
    uint64_t allocated_ = 0;
    uint64_t chunk_id_counter_ = 2; // 0 invalid, 1 is initial chunk
    std::unordered_map<uint64_t, MemoryChunk> chunks_;
    std::unordered_set<uint64_t> free_chunks_;
};

// ============================================================================
// Memory Block
// ============================================================================

struct SendSyncPtr {
    void* Ptr = nullptr;

    SendSyncPtr() = default;
    explicit SendSyncPtr(void* p) : Ptr(p) {}
};

class Allocator::MemoryBlock {
public:
    VkDeviceMemory DeviceMemory = VK_NULL_HANDLE;
    uint64_t Size = 0;
    std::optional<SendSyncPtr> MappedPtr;
    std::unique_ptr<SubAllocator> SubAllocator;
    bool DedicatedAllocation = false;

    static Result<std::unique_ptr<MemoryBlock>> New(
        VkDevice device,
        uint64_t size,
        size_t mem_type_index,
        bool mapped,
        bool buffer_device_address,
        AllocationScheme scheme,
        VkBuffer dedicated_buffer,
        VkImage dedicated_image,
        bool requires_personal_block
    );

    void Destroy(VkDevice device);

private:
    MemoryBlock() = default;
};

// ============================================================================
// Memory Type
// ============================================================================

class Allocator::MemoryType {
public:
    std::vector<std::unique_ptr<MemoryBlock>> MemoryBlocks;
    VkMemoryPropertyFlags MemoryProperties = 0;
    size_t MemoryTypeIndex = 0;
    size_t HeapIndex = 0;
    bool Mappable = false;
    size_t ActiveGeneralBlocks = 0;
    bool BufferDeviceAddress = false;

    Result<Allocation> Allocate(
        VkDevice device,
        const AllocationCreateDesc& desc,
        uint64_t granularity,
        const AllocationSizes& allocation_sizes
    );

    ResultVoid Free(Allocation allocation, VkDevice device);
};

// ============================================================================
// Utility Functions
// ============================================================================

inline uint64_t AlignDown(uint64_t val, uint64_t alignment) {
    return val & ~(alignment - 1);
}

inline uint64_t AlignUp(uint64_t val, uint64_t alignment) {
    return AlignDown(val + alignment - 1, alignment);
}

inline bool IsOnSamePage(uint64_t offset_a, uint64_t size_a, uint64_t offset_b, uint64_t page_size) {
    const uint64_t end_a = offset_a + size_a - 1;
    const uint64_t end_page_a = AlignDown(end_a, page_size);
    const uint64_t start_page_b = AlignDown(offset_b, page_size);
    return end_page_a == start_page_b;
}

inline bool HasGranularityConflict(AllocationType type0, AllocationType type1) {
    if (type0 == AllocationType::Free || type1 == AllocationType::Free) {
        return false;
    }
    return type0 != type1;
}

std::string FmtBytes(uint64_t amount);

} // namespace tekki::gpu_allocator
