#include <tekki/gpu_allocator/gpu_allocator.h>
#include "allocator_internal.h"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace tekki::gpu_allocator {

// ============================================================================
// MemoryBlock Implementation
// ============================================================================

Result<std::unique_ptr<Allocator::MemoryBlock>> Allocator::MemoryBlock::New(
    VkDevice device,
    uint64_t size,
    size_t mem_type_index,
    bool mapped,
    bool buffer_device_address,
    AllocationScheme scheme,
    VkBuffer dedicated_buffer,
    VkImage dedicated_image,
    bool requires_personal_block)
{
    auto block = std::make_unique<MemoryBlock>();
    block->Size = size;

    // Allocate device memory
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = size;
    alloc_info.memoryTypeIndex = static_cast<uint32_t>(mem_type_index);

    VkMemoryAllocateFlagsInfo flags_info = {};
    if (buffer_device_address) {
        flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        alloc_info.pNext = &flags_info;
    }

    VkMemoryDedicatedAllocateInfo dedicated_info = {};
    if (scheme == AllocationScheme::DedicatedBuffer) {
        dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicated_info.buffer = dedicated_buffer;
        dedicated_info.pNext = alloc_info.pNext;
        alloc_info.pNext = &dedicated_info;
    } else if (scheme == AllocationScheme::DedicatedImage) {
        dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicated_info.image = dedicated_image;
        dedicated_info.pNext = alloc_info.pNext;
        alloc_info.pNext = &dedicated_info;
    }

    VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &block->DeviceMemory);
    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY) {
        return AllocationError::OutOfMemory();
    }
    if (result != VK_SUCCESS) {
        return AllocationError::Internal("Unexpected error in vkAllocateMemory: " + std::to_string(result));
    }

    // Map memory if requested
    if (mapped) {
        void* mapped_ptr = nullptr;
        result = vkMapMemory(device, block->DeviceMemory, 0, VK_WHOLE_SIZE, 0, &mapped_ptr);
        if (result != VK_SUCCESS) {
            vkFreeMemory(device, block->DeviceMemory, nullptr);
            return AllocationError::FailedToMap("vkMapMemory failed: " + std::to_string(result));
        }

        if (mapped_ptr == nullptr) {
            vkFreeMemory(device, block->DeviceMemory, nullptr);
            return AllocationError::FailedToMap("Returned mapped pointer is null");
        }

        block->MappedPtr = SendSyncPtr(mapped_ptr);
    }

    // Create sub-allocator
    if (scheme != AllocationScheme::GpuAllocatorManaged || requires_personal_block) {
        block->SubAllocator = std::make_unique<DedicatedBlockAllocator>(size);
        block->DedicatedAllocation = true;
    } else {
        block->SubAllocator = std::make_unique<FreeListAllocator>(size);
        block->DedicatedAllocation = false;
    }

    return block;
}

void Allocator::MemoryBlock::Destroy(VkDevice device) {
    if (MappedPtr.has_value()) {
        vkUnmapMemory(device, DeviceMemory);
    }
    vkFreeMemory(device, DeviceMemory, nullptr);
}

// ============================================================================
// MemoryType Implementation
// ============================================================================

Result<Allocation> Allocator::MemoryType::Allocate(
    VkDevice device,
    const AllocationCreateDesc& desc,
    uint64_t granularity,
    const AllocationSizes& allocation_sizes)
{
    const AllocationType allocation_type = desc.Linear ? AllocationType::Linear : AllocationType::NonLinear;

    const bool is_host = (MemoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    const uint64_t memblock_size = allocation_sizes.GetMemblockSize(is_host, ActiveGeneralBlocks);

    const uint64_t size = desc.Requirements.size;
    const uint64_t alignment = desc.Requirements.alignment;

    const bool dedicated_allocation = desc.Scheme != AllocationScheme::GpuAllocatorManaged;
    const bool requires_personal_block = size > memblock_size;

    // Create a dedicated block for large allocations or allocations that require dedicated memory
    if (dedicated_allocation || requires_personal_block) {
        auto mem_block_result = MemoryBlock::New(
            device, size, MemoryTypeIndex, Mappable,
            BufferDeviceAddress, desc.Scheme,
            desc.DedicatedBuffer, desc.DedicatedImage,
            requires_personal_block
        );

        if (IsErr(mem_block_result)) {
            return UnwrapErr(mem_block_result);
        }

        auto mem_block = Unwrap(mem_block_result);

        // Find an empty slot
        size_t block_index = MemoryBlocks.size();
        for (size_t i = 0; i < MemoryBlocks.size(); ++i) {
            if (!MemoryBlocks[i]) {
                block_index = i;
                break;
            }
        }

        if (block_index == MemoryBlocks.size()) {
            MemoryBlocks.push_back(std::move(mem_block));
        } else {
            MemoryBlocks[block_index] = std::move(mem_block);
        }

        auto& block = MemoryBlocks[block_index];

        auto alloc_result = block->SubAllocator->Allocate(
            size, alignment, allocation_type, granularity, desc.Name
        );

        if (IsErr(alloc_result)) {
            return UnwrapErr(alloc_result);
        }

        const auto [offset, chunk_id] = Unwrap(alloc_result);

        Allocation allocation;
        allocation.chunk_id_ = chunk_id;
        allocation.offset_ = offset;
        allocation.size_ = size;
        allocation.memory_block_index_ = block_index;
        allocation.memory_type_index_ = MemoryTypeIndex;
        allocation.device_memory_ = block->DeviceMemory;
        allocation.mapped_ptr_ = block->MappedPtr.has_value() ? block->MappedPtr->Ptr : nullptr;
        allocation.memory_properties_ = MemoryProperties;
        allocation.name_ = desc.Name;
        allocation.dedicated_allocation_ = dedicated_allocation;

        return allocation;
    }

    // Try to allocate from existing blocks
    std::optional<size_t> empty_block_index;
    for (size_t mem_block_i = MemoryBlocks.size(); mem_block_i > 0; --mem_block_i) {
        const size_t i = mem_block_i - 1;

        if (MemoryBlocks[i]) {
            auto& mem_block = MemoryBlocks[i];

            auto alloc_result = mem_block->SubAllocator->Allocate(
                size, alignment, allocation_type, granularity, desc.Name
            );

            if (IsOk(alloc_result)) {
                const auto [offset, chunk_id] = Unwrap(alloc_result);

                void* mapped_ptr = nullptr;
                if (mem_block->MappedPtr.has_value()) {
                    mapped_ptr = static_cast<uint8_t*>(mem_block->MappedPtr->Ptr) + offset;
                }

                Allocation allocation;
                allocation.chunk_id_ = chunk_id;
                allocation.offset_ = offset;
                allocation.size_ = size;
                allocation.memory_block_index_ = i;
                allocation.memory_type_index_ = MemoryTypeIndex;
                allocation.device_memory_ = mem_block->DeviceMemory;
                allocation.memory_properties_ = MemoryProperties;
                allocation.mapped_ptr_ = mapped_ptr;
                allocation.dedicated_allocation_ = false;
                allocation.name_ = desc.Name;

                return allocation;
            } else {
                auto& err = UnwrapErr(alloc_result);
                if (err.Type != AllocationErrorType::OutOfMemory) {
                    return err; // Unhandled error
                }
                // Block is full, continue search
            }
        } else if (!empty_block_index.has_value()) {
            empty_block_index = i;
        }
    }

    // Create a new memory block
    auto new_memory_block_result = MemoryBlock::New(
        device, memblock_size, MemoryTypeIndex, Mappable,
        BufferDeviceAddress, desc.Scheme,
        VK_NULL_HANDLE, VK_NULL_HANDLE, false
    );

    if (IsErr(new_memory_block_result)) {
        return UnwrapErr(new_memory_block_result);
    }

    auto new_memory_block = Unwrap(new_memory_block_result);

    size_t new_block_index;
    if (empty_block_index.has_value()) {
        new_block_index = empty_block_index.value();
        MemoryBlocks[new_block_index] = std::move(new_memory_block);
    } else {
        new_block_index = MemoryBlocks.size();
        MemoryBlocks.push_back(std::move(new_memory_block));
    }

    ActiveGeneralBlocks++;

    auto& mem_block = MemoryBlocks[new_block_index];

    auto alloc_result = mem_block->SubAllocator->Allocate(
        size, alignment, allocation_type, granularity, desc.Name
    );

    if (IsErr(alloc_result)) {
        auto& err = UnwrapErr(alloc_result);
        if (err.Type == AllocationErrorType::OutOfMemory) {
            return AllocationError::Internal(
                "Allocation that must succeed failed. This is a bug in the allocator."
            );
        }
        return err;
    }

    const auto [offset, chunk_id] = Unwrap(alloc_result);

    void* mapped_ptr = nullptr;
    if (mem_block->MappedPtr.has_value()) {
        mapped_ptr = static_cast<uint8_t*>(mem_block->MappedPtr->Ptr) + offset;
    }

    Allocation allocation;
    allocation.chunk_id_ = chunk_id;
    allocation.offset_ = offset;
    allocation.size_ = size;
    allocation.memory_block_index_ = new_block_index;
    allocation.memory_type_index_ = MemoryTypeIndex;
    allocation.device_memory_ = mem_block->DeviceMemory;
    allocation.mapped_ptr_ = mapped_ptr;
    allocation.memory_properties_ = MemoryProperties;
    allocation.name_ = desc.Name;
    allocation.dedicated_allocation_ = false;

    return allocation;
}

ResultVoid Allocator::MemoryType::Free(Allocation allocation, VkDevice device) {
    const size_t block_idx = allocation.memory_block_index_;

    if (block_idx >= MemoryBlocks.size() || !MemoryBlocks[block_idx]) {
        return AllocationError::Internal("Memory block must be valid.");
    }

    auto& mem_block = MemoryBlocks[block_idx];

    auto free_result = mem_block->SubAllocator->Free(allocation.chunk_id_);
    if (IsErr(free_result)) {
        return UnwrapErr(free_result);
    }

    // Destroy the block if it's empty and it's either dedicated or not the last general block
    const bool is_dedicated_or_not_last_general_block =
        !mem_block->SubAllocator->SupportsGeneralAllocations() || ActiveGeneralBlocks > 1;

    if (mem_block->SubAllocator->IsEmpty() && is_dedicated_or_not_last_general_block) {
        if (mem_block->SubAllocator->SupportsGeneralAllocations()) {
            ActiveGeneralBlocks--;
        }

        mem_block->Destroy(device);
        MemoryBlocks[block_idx].reset();
    }

    return Ok();
}

// ============================================================================
// Allocator Implementation
// ============================================================================

Result<std::unique_ptr<Allocator>> Allocator::New(const AllocatorCreateDesc& desc) {
    if (desc.PhysicalDevice == VK_NULL_HANDLE) {
        return AllocationError::InvalidAllocatorCreateDesc(
            "AllocatorCreateDesc field `PhysicalDevice` is null."
        );
    }

    auto allocator = std::unique_ptr<Allocator>(new Allocator());

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(desc.PhysicalDevice, &mem_props);

    allocator->memory_heaps_.assign(
        mem_props.memoryHeaps,
        mem_props.memoryHeaps + mem_props.memoryHeapCount
    );

    if (desc.DebugSettings.LogMemoryInformation) {
        spdlog::debug("memory type count: {}", mem_props.memoryTypeCount);
        spdlog::debug("memory heap count: {}", mem_props.memoryHeapCount);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            const auto& mem_type = mem_props.memoryTypes[i];
            spdlog::debug("memory type[{}]: prop flags: 0x{:x}, heap[{}]",
                         i, mem_type.propertyFlags, mem_type.heapIndex);
        }

        for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
            const auto& heap = mem_props.memoryHeaps[i];
            spdlog::debug("heap[{}] flags: 0x{:x}, size: {} MiB",
                         i, heap.flags, heap.size / (1024 * 1024));
        }
    }

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        const auto& mem_type = mem_props.memoryTypes[i];

        auto memory_type = std::make_unique<MemoryType>();
        memory_type->MemoryProperties = mem_type.propertyFlags;
        memory_type->MemoryTypeIndex = i;
        memory_type->HeapIndex = mem_type.heapIndex;
        memory_type->Mappable = (mem_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        memory_type->ActiveGeneralBlocks = 0;
        memory_type->BufferDeviceAddress = desc.BufferDeviceAddress;

        allocator->memory_types_.push_back(std::move(memory_type));
    }

    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(desc.PhysicalDevice, &physical_device_properties);

    allocator->device_ = desc.Device;
    allocator->buffer_image_granularity_ = physical_device_properties.limits.bufferImageGranularity;
    allocator->debug_settings_ = desc.DebugSettings;
    allocator->allocation_sizes_ = desc.AllocationSizes;

    return allocator;
}

Allocator::~Allocator() {
    if (debug_settings_.LogLeaksOnShutdown) {
        for (size_t mem_type_i = 0; mem_type_i < memory_types_.size(); ++mem_type_i) {
            auto& mem_type = memory_types_[mem_type_i];
            for (size_t block_i = 0; block_i < mem_type->MemoryBlocks.size(); ++block_i) {
                if (mem_type->MemoryBlocks[block_i]) {
                    mem_type->MemoryBlocks[block_i]->SubAllocator->ReportMemoryLeaks(mem_type_i, block_i);
                }
            }
        }
    }

    // Free all remaining memory blocks
    for (auto& mem_type : memory_types_) {
        for (auto& mem_block : mem_type->MemoryBlocks) {
            if (mem_block) {
                mem_block->Destroy(device_);
            }
        }
    }
}

Result<Allocation> Allocator::Allocate(const AllocationCreateDesc& desc) {
    const uint64_t size = desc.Requirements.size;
    const uint64_t alignment = desc.Requirements.alignment;

    if (debug_settings_.LogAllocations) {
        spdlog::debug("Allocating `{}` of {} bytes with an alignment of {}.",
                     desc.Name, size, alignment);
    }

    if (size == 0 || (alignment & (alignment - 1)) != 0) {
        return AllocationError::InvalidAllocationCreateDesc();
    }

    // Determine preferred memory property flags
    VkMemoryPropertyFlags mem_loc_preferred_bits = 0;
    switch (desc.Location) {
        case MemoryLocation::GpuOnly:
            mem_loc_preferred_bits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case MemoryLocation::CpuToGpu:
            mem_loc_preferred_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case MemoryLocation::GpuToCpu:
            mem_loc_preferred_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            break;
        case MemoryLocation::Unknown:
            mem_loc_preferred_bits = 0;
            break;
    }

    // Find memory type index with preferred flags
    auto find_memory_type = [&](VkMemoryPropertyFlags flags) -> std::optional<uint32_t> {
        for (const auto& memory_type : memory_types_) {
            const uint32_t type_bit = (1u << memory_type->MemoryTypeIndex);
            if ((type_bit & desc.Requirements.memoryTypeBits) != 0 &&
                (memory_type->MemoryProperties & flags) == flags)
            {
                return static_cast<uint32_t>(memory_type->MemoryTypeIndex);
            }
        }
        return std::nullopt;
    };

    auto memory_type_index_opt = find_memory_type(mem_loc_preferred_bits);

    // Fallback to required flags
    if (!memory_type_index_opt.has_value()) {
        VkMemoryPropertyFlags mem_loc_required_bits = 0;
        switch (desc.Location) {
            case MemoryLocation::GpuOnly:
                mem_loc_required_bits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case MemoryLocation::CpuToGpu:
            case MemoryLocation::GpuToCpu:
                mem_loc_required_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case MemoryLocation::Unknown:
                mem_loc_required_bits = 0;
                break;
        }
        memory_type_index_opt = find_memory_type(mem_loc_required_bits);
    }

    if (!memory_type_index_opt.has_value()) {
        return AllocationError::NoCompatibleMemoryTypeFound();
    }

    const size_t memory_type_index = memory_type_index_opt.value();

    // Check heap size
    auto& memory_type = memory_types_[memory_type_index];
    if (size > memory_heaps_[memory_type->HeapIndex].size) {
        return AllocationError::OutOfMemory();
    }

    auto allocation = memory_type->Allocate(
        device_, desc, buffer_image_granularity_, allocation_sizes_
    );

    // Fallback for CpuToGpu
    if (desc.Location == MemoryLocation::CpuToGpu && IsErr(allocation)) {
        VkMemoryPropertyFlags fallback_flags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        auto fallback_type_opt = find_memory_type(fallback_flags);
        if (fallback_type_opt.has_value()) {
            const size_t fallback_index = fallback_type_opt.value();
            allocation = memory_types_[fallback_index]->Allocate(
                device_, desc, buffer_image_granularity_, allocation_sizes_
            );
        }
    }

    return allocation;
}

ResultVoid Allocator::Free(Allocation allocation) {
    if (debug_settings_.LogFrees) {
        const std::string& name = allocation.name_.value_or("<null>");
        spdlog::debug("Freeing `{}`.", name);
    }

    if (allocation.IsNull()) {
        return Ok();
    }

    return memory_types_[allocation.memory_type_index_]->Free(std::move(allocation), device_);
}

ResultVoid Allocator::RenameAllocation(Allocation& allocation, const std::string& name) {
    allocation.name_ = name;

    if (allocation.IsNull()) {
        return Ok();
    }

    auto& mem_type = memory_types_[allocation.memory_type_index_];
    if (allocation.memory_block_index_ >= mem_type->MemoryBlocks.size() ||
        !mem_type->MemoryBlocks[allocation.memory_block_index_])
    {
        return AllocationError::Internal("Memory block must be valid.");
    }

    auto& mem_block = mem_type->MemoryBlocks[allocation.memory_block_index_];
    return mem_block->SubAllocator->RenameAllocation(allocation.chunk_id_, name);
}

void Allocator::ReportMemoryLeaks() const {
    for (size_t mem_type_i = 0; mem_type_i < memory_types_.size(); ++mem_type_i) {
        auto& mem_type = memory_types_[mem_type_i];
        for (size_t block_i = 0; block_i < mem_type->MemoryBlocks.size(); ++block_i) {
            if (mem_type->MemoryBlocks[block_i]) {
                mem_type->MemoryBlocks[block_i]->SubAllocator->ReportMemoryLeaks(mem_type_i, block_i);
            }
        }
    }
}

AllocatorReport Allocator::GenerateReport() const {
    AllocatorReport report;

    for (const auto& memory_type : memory_types_) {
        for (const auto& block : memory_type->MemoryBlocks) {
            if (!block) continue;

            report.TotalCapacityBytes += block->Size;

            const size_t first_allocation = report.Allocations.size();
            auto block_allocations = block->SubAllocator->ReportAllocations();
            report.Allocations.insert(
                report.Allocations.end(),
                block_allocations.begin(),
                block_allocations.end()
            );

            MemoryBlockReport block_report;
            block_report.Size = block->Size;
            block_report.FirstAllocation = first_allocation;
            block_report.AllocationCount = block_allocations.size();
            report.Blocks.push_back(block_report);
        }
    }

    for (const auto& alloc : report.Allocations) {
        report.TotalAllocatedBytes += alloc.Size;
    }

    return report;
}

uint64_t Allocator::Capacity() const {
    uint64_t total_capacity_bytes = 0;

    for (const auto& memory_type : memory_types_) {
        for (const auto& block : memory_type->MemoryBlocks) {
            if (block) {
                total_capacity_bytes += block->Size;
            }
        }
    }

    return total_capacity_bytes;
}

} // namespace tekki::gpu_allocator
