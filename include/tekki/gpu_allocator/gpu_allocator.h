#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <vulkan/vulkan.h>

namespace tekki::gpu_allocator
{

// ============================================================================
// Error Handling
// ============================================================================

enum class AllocationErrorType
{
    OutOfMemory,
    FailedToMap,
    NoCompatibleMemoryTypeFound,
    InvalidAllocationCreateDesc,
    InvalidAllocatorCreateDesc,
    Internal,
};

class AllocationError
{
public:
    AllocationErrorType Type;
    std::string Message;

    explicit AllocationError(AllocationErrorType type, std::string message = "")
        : Type(type), Message(std::move(message))
    {
    }

    static AllocationError OutOfMemory() { return AllocationError(AllocationErrorType::OutOfMemory, "Out of memory"); }

    static AllocationError FailedToMap(const std::string& msg)
    {
        return AllocationError(AllocationErrorType::FailedToMap, "Failed to map memory: " + msg);
    }

    static AllocationError NoCompatibleMemoryTypeFound()
    {
        return AllocationError(AllocationErrorType::NoCompatibleMemoryTypeFound, "No compatible memory type available");
    }

    static AllocationError InvalidAllocationCreateDesc()
    {
        return AllocationError(AllocationErrorType::InvalidAllocationCreateDesc, "Invalid AllocationCreateDesc");
    }

    static AllocationError InvalidAllocatorCreateDesc(const std::string& msg)
    {
        return AllocationError(AllocationErrorType::InvalidAllocatorCreateDesc, msg);
    }

    static AllocationError Internal(const std::string& msg)
    {
        return AllocationError(AllocationErrorType::Internal, "Internal error: " + msg);
    }
};

template <typename T> using Result = std::variant<T, AllocationError>;

template <typename T> inline bool IsOk(const Result<T>& result)
{
    return std::holds_alternative<T>(result);
}

template <typename T> inline bool IsErr(const Result<T>& result)
{
    return std::holds_alternative<AllocationError>(result);
}

template <typename T> inline T& Unwrap(Result<T>& result)
{
    return std::get<T>(result);
}

template <typename T> inline const T& Unwrap(const Result<T>& result)
{
    return std::get<T>(result);
}

template <typename T> inline T&& Unwrap(Result<T>&& result)
{
    return std::get<T>(std::move(result));
}

template <typename T> inline AllocationError& UnwrapErr(Result<T>& result)
{
    return std::get<AllocationError>(result);
}

using ResultVoid = Result<std::monostate>;

inline ResultVoid Ok()
{
    return std::monostate{};
}

inline ResultVoid Err(AllocationError error)
{
    return error;
}

// ============================================================================
// Core Types
// ============================================================================

enum class MemoryLocation
{
    /// The allocated resource is stored at an unknown memory location; let the driver decide
    Unknown,
    /// GPU only accessible memory - typically faster, where most allocations should live
    GpuOnly,
    /// Memory useful for uploading data to the GPU and potentially for constant buffers
    CpuToGpu,
    /// Memory useful for CPU readback of data
    GpuToCpu,
};

struct AllocatorDebugSettings
{
    /// Logs out debugging information about the various heaps the current device has on startup
    bool LogMemoryInformation = false;
    /// Logs out all memory leaks on shutdown with log level Warn
    bool LogLeaksOnShutdown = true;
    /// Log out every allocation as it's being made with log level Debug
    bool LogAllocations = false;
    /// Log out every free that is being called with log level Debug
    bool LogFrees = false;
};

/// The sizes of the memory blocks that the allocator will create.
class AllocationSizes
{
public:
    static constexpr uint64_t MB = 1024 * 1024;

    AllocationSizes() = default;

    AllocationSizes(uint64_t device_memblock_size, uint64_t host_memblock_size)
        : min_device_memblock_size_(AdjustMemblockSize(device_memblock_size, "Device")),
          max_device_memblock_size_(min_device_memblock_size_),
          min_host_memblock_size_(AdjustMemblockSize(host_memblock_size, "Host")),
          max_host_memblock_size_(min_host_memblock_size_)
    {
    }

    AllocationSizes& WithMaxDeviceMemblockSize(uint64_t size)
    {
        max_device_memblock_size_ = std::max(AdjustMemblockSize(size, "Device"), min_device_memblock_size_);
        return *this;
    }

    AllocationSizes& WithMaxHostMemblockSize(uint64_t size)
    {
        max_host_memblock_size_ = std::max(AdjustMemblockSize(size, "Host"), min_host_memblock_size_);
        return *this;
    }

    uint64_t GetMemblockSize(bool is_host, size_t count) const
    {
        const auto [min_size, max_size] = is_host
                                              ? std::make_pair(min_host_memblock_size_, max_host_memblock_size_)
                                              : std::make_pair(min_device_memblock_size_, max_device_memblock_size_);

        const uint64_t shift = std::min<uint64_t>(count, 7);
        return std::min(min_size << shift, max_size);
    }

    static AllocationSizes Default() { return AllocationSizes(256 * MB, 64 * MB); }

private:
    uint64_t min_device_memblock_size_ = 256 * MB;
    uint64_t max_device_memblock_size_ = 256 * MB;
    uint64_t min_host_memblock_size_ = 64 * MB;
    uint64_t max_host_memblock_size_ = 64 * MB;

    static uint64_t AdjustMemblockSize(uint64_t size, const char* kind);
};

// ============================================================================
// Vulkan-specific types
// ============================================================================

enum class AllocationScheme
{
    /// Perform a dedicated allocation for the given buffer
    DedicatedBuffer,
    /// Perform a dedicated allocation for the given image
    DedicatedImage,
    /// The memory for this resource will be allocated and managed by gpu-allocator
    GpuAllocatorManaged,
};

struct AllocationCreateDesc
{
    /// Name of the allocation, for tracking and debugging purposes
    std::string Name;
    /// Vulkan memory requirements for an allocation
    VkMemoryRequirements Requirements = {};
    /// Location where the memory allocation should be stored
    MemoryLocation Location = MemoryLocation::Unknown;
    /// If the resource is linear (buffer/linear texture) or regular (tiled) texture
    bool Linear = true;
    /// Determines how this allocation should be managed
    AllocationScheme Scheme = AllocationScheme::GpuAllocatorManaged;
    /// Vulkan buffer handle for dedicated allocations
    VkBuffer DedicatedBuffer = VK_NULL_HANDLE;
    /// Vulkan image handle for dedicated allocations
    VkImage DedicatedImage = VK_NULL_HANDLE;
};

/// A piece of allocated memory
class Allocation
{
public:
    Allocation() = default;

    bool IsNull() const { return !chunk_id_.has_value(); }

    std::optional<uint64_t> ChunkId() const { return chunk_id_; }
    VkMemoryPropertyFlags MemoryProperties() const { return memory_properties_; }
    VkDeviceMemory Memory() const { return device_memory_; }
    bool IsDedicated() const { return dedicated_allocation_; }
    uint64_t Offset() const { return offset_; }
    uint64_t Size() const { return size_; }
    void* MappedPtr() const { return mapped_ptr_; }

    /// Returns a valid mapped slice if the memory is host visible
    const uint8_t* MappedSlice() const { return static_cast<const uint8_t*>(mapped_ptr_); }

    /// Returns a valid mutable mapped slice if the memory is host visible
    uint8_t* MappedSliceMut() { return static_cast<uint8_t*>(mapped_ptr_); }

private:
    friend class Allocator;
    friend class MemoryType;

    std::optional<uint64_t> chunk_id_;
    uint64_t offset_ = 0;
    uint64_t size_ = 0;
    size_t memory_block_index_ = ~0u;
    size_t memory_type_index_ = ~0u;
    VkDeviceMemory device_memory_ = VK_NULL_HANDLE;
    void* mapped_ptr_ = nullptr;
    bool dedicated_allocation_ = false;
    VkMemoryPropertyFlags memory_properties_ = 0;
    std::optional<std::string> name_;
};

struct AllocatorCreateDesc
{
    VkInstance Instance = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    AllocatorDebugSettings DebugSettings = {};
    bool BufferDeviceAddress = true;
    AllocationSizes AllocationSizes = AllocationSizes::Default();
};

/// Describes an allocation in the AllocatorReport
struct AllocationReport
{
    std::string Name;
    uint64_t Offset = 0;
    uint64_t Size = 0;
};

/// Describes a memory block in the AllocatorReport
struct MemoryBlockReport
{
    uint64_t Size = 0;
    std::pair<size_t, size_t> AllocationsRange; // start, end indices in allocations vector
};

/// A report that can be generated for informational purposes using Allocator::GenerateReport()
struct AllocatorReport
{
    std::vector<AllocationReport> Allocations;
    std::vector<MemoryBlockReport> Blocks;
    uint64_t TotalAllocatedBytes = 0;
    uint64_t TotalCapacityBytes = 0;
};

/// Vulkan memory allocator
class Allocator
{
public:
    static Result<std::unique_ptr<Allocator>> New(const AllocatorCreateDesc& desc);

    ~Allocator();

    // Disable copy
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    /// Allocate memory
    Result<Allocation> Allocate(const AllocationCreateDesc& desc);

    /// Free an allocation
    ResultVoid Free(Allocation allocation);

    /// Rename an allocation (for debugging purposes)
    ResultVoid RenameAllocation(Allocation& allocation, const std::string& name);

    /// Report memory leaks at a specific log level
    void ReportMemoryLeaks() const;

    /// Generate a report of current allocations and memory usage
    AllocatorReport GenerateReport() const;

    /// Get current total capacity of allocated memory blocks, in bytes
    uint64_t Capacity() const;

private:
    Allocator() = default;

    class MemoryBlock;
    class MemoryType;

    std::vector<std::unique_ptr<MemoryType>> memory_types_;
    std::vector<VkMemoryHeap> memory_heaps_;
    VkDevice device_ = VK_NULL_HANDLE;
    uint64_t buffer_image_granularity_ = 0;
    AllocatorDebugSettings debug_settings_;
    AllocationSizes allocation_sizes_;
};

} // namespace tekki::gpu_allocator
