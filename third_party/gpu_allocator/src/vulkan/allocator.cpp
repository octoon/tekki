#include "tekki/gpu_allocator/vulkan/allocator.h"
#include "tekki/gpu_allocator/allocator/dedicated_block_allocator.h"
#include "tekki/gpu_allocator/allocator/free_list_allocator.h"
#include <spdlog/spdlog.h>

namespace tekki {

// Allocation 移动构造和移动赋值
Allocation::Allocation(Allocation&& other) noexcept
    : chunk_id_(other.chunk_id_),
      offset_(other.offset_),
      size_(other.size_),
      memory_block_index_(other.memory_block_index_),
      memory_type_index_(other.memory_type_index_),
      device_memory_(other.device_memory_),
      mapped_ptr_(other.mapped_ptr_),
      dedicated_allocation_(other.dedicated_allocation_),
      memory_properties_(other.memory_properties_),
      name_(std::move(other.name_)) {
    other.chunk_id_ = std::nullopt;
    other.device_memory_ = VK_NULL_HANDLE;
}

Allocation& Allocation::operator=(Allocation&& other) noexcept {
    if (this != &other) {
        chunk_id_ = other.chunk_id_;
        offset_ = other.offset_;
        size_ = other.size_;
        memory_block_index_ = other.memory_block_index_;
        memory_type_index_ = other.memory_type_index_;
        device_memory_ = other.device_memory_;
        mapped_ptr_ = other.mapped_ptr_;
        dedicated_allocation_ = other.dedicated_allocation_;
        memory_properties_ = other.memory_properties_;
        name_ = std::move(other.name_);

        other.chunk_id_ = std::nullopt;
        other.device_memory_ = VK_NULL_HANDLE;
    }
    return *this;
}

std::optional<uint64_t> Allocation::ChunkId() const {
    return chunk_id_;
}

VkMemoryPropertyFlags Allocation::MemoryProperties() const {
    return memory_properties_;
}

VkDeviceMemory Allocation::Memory() const {
    return device_memory_;
}

bool Allocation::IsDedicated() const {
    return dedicated_allocation_;
}

uint64_t Allocation::Offset() const {
    return offset_;
}

uint64_t Allocation::Size() const {
    return size_;
}

void* Allocation::MappedPtr() const {
    return mapped_ptr_;
}

uint8_t* Allocation::MappedSlice() {
    return static_cast<uint8_t*>(mapped_ptr_);
}

const uint8_t* Allocation::MappedSlice() const {
    return static_cast<const uint8_t*>(mapped_ptr_);
}

bool Allocation::IsNull() const {
    return !chunk_id_.has_value();
}

// MemoryBlock 创建
[[maybe_unused]] static MemoryBlock CreateMemoryBlock(
    VkDevice device,
    uint64_t size,
    uint32_t mem_type_index,
    bool mapped,
    bool buffer_device_address,
    const AllocationCreateDesc& desc,
    bool requires_personal_block) {

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = size;
    alloc_info.memoryTypeIndex = mem_type_index;

    // Buffer device address 标志
    VkMemoryAllocateFlags allocation_flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateFlagsInfo flags_info{};
    flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags_info.flags = allocation_flags;

    if (buffer_device_address) {
        alloc_info.pNext = &flags_info;
    }

    // 专用分配标志
    VkMemoryDedicatedAllocateInfo dedicated_info{};
    dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;

    if (desc.allocation_scheme == AllocationScheme::DedicatedBuffer) {
        dedicated_info.buffer = desc.dedicated_buffer;
        flags_info.pNext = &dedicated_info;
        alloc_info.pNext = &flags_info;
    } else if (desc.allocation_scheme == AllocationScheme::DedicatedImage) {
        dedicated_info.image = desc.dedicated_image;
        flags_info.pNext = &dedicated_info;
        alloc_info.pNext = &flags_info;
    }

    VkDeviceMemory device_memory;
    VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &device_memory);

    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        throw AllocationError(AllocationErrorCode::OutOfMemory);
    } else if (result != VK_SUCCESS) {
        throw AllocationError(
            AllocationErrorCode::Internal,
            "Unexpected error in vkAllocateMemory"
        );
    }

    void* mapped_ptr = nullptr;
    if (mapped) {
        result = vkMapMemory(device, device_memory, 0, VK_WHOLE_SIZE, 0, &mapped_ptr);
        if (result != VK_SUCCESS) {
            vkFreeMemory(device, device_memory, nullptr);
            throw AllocationError(AllocationErrorCode::FailedToMap);
        }

        if (mapped_ptr == nullptr) {
            vkFreeMemory(device, device_memory, nullptr);
            throw AllocationError(
                AllocationErrorCode::FailedToMap,
                "Returned mapped pointer is null"
            );
        }
    }

    // 创建子分配器
    std::unique_ptr<SubAllocator> sub_allocator;
    if (desc.allocation_scheme != AllocationScheme::GpuAllocatorManaged ||
        requires_personal_block) {
        sub_allocator = std::make_unique<DedicatedBlockAllocator>(size);
    } else {
        sub_allocator = std::make_unique<FreeListAllocator>(size);
    }

    return MemoryBlock{
        device_memory,
        size,
        mapped_ptr,
        std::move(sub_allocator)
    };
}

// Allocator 构造函数
Allocator::Allocator(const AllocatorCreateDesc& desc)
    : device_(desc.device),
      debug_settings_(desc.debug_settings),
      allocation_sizes_(desc.allocation_sizes) {

    if (desc.physical_device == VK_NULL_HANDLE) {
        throw AllocationError(
            AllocationErrorCode::InvalidAllocatorCreateDesc,
            "AllocatorCreateDesc field `physical_device` is null."
        );
    }

    // 获取内存属性
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(desc.physical_device, &mem_props);

    // 记录内存信息
    if (debug_settings_.log_memory_information) {
        spdlog::debug("memory type count: {}", mem_props.memoryTypeCount);
        spdlog::debug("memory heap count: {}", mem_props.memoryHeapCount);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            spdlog::debug("memory type[{}]: prop flags: 0x{:x}, heap[{}]",
                i,
                static_cast<uint32_t>(mem_props.memoryTypes[i].propertyFlags),
                mem_props.memoryTypes[i].heapIndex);
        }

        for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
            spdlog::debug("heap[{}] flags: 0x{:x}, size: {} MiB",
                i,
                static_cast<uint32_t>(mem_props.memoryHeaps[i].flags),
                mem_props.memoryHeaps[i].size / (1024 * 1024));
        }
    }

    // 初始化内存类型
    memory_types_.reserve(mem_props.memoryTypeCount);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        MemoryType mem_type{};
        mem_type.memory_properties = mem_props.memoryTypes[i].propertyFlags;
        mem_type.memory_type_index = i;
        mem_type.heap_index = mem_props.memoryTypes[i].heapIndex;
        mem_type.mappable = (mem_props.memoryTypes[i].propertyFlags &
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        mem_type.active_general_blocks = 0;
        mem_type.buffer_device_address = desc.buffer_device_address;

        memory_types_.push_back(std::move(mem_type));
    }

    // 保存堆信息
    memory_heaps_.reserve(mem_props.memoryHeapCount);
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        memory_heaps_.push_back(mem_props.memoryHeaps[i]);
    }

    // 获取粒度
    VkPhysicalDeviceProperties dev_props;
    vkGetPhysicalDeviceProperties(desc.physical_device, &dev_props);
    buffer_image_granularity_ = dev_props.limits.bufferImageGranularity;
}

Allocator::~Allocator() {
    if (debug_settings_.log_leaks_on_shutdown) {
        ReportMemoryLeaks(static_cast<int>(spdlog::level::warn));
    }

    // 释放所有内存块
    for (auto& mem_type : memory_types_) {
        for (auto& mem_block_opt : mem_type.memory_blocks) {
            if (mem_block_opt.has_value()) {
                auto& mem_block = mem_block_opt.value();
                if (mem_block.mapped_ptr != nullptr) {
                    vkUnmapMemory(device_, mem_block.device_memory);
                }
                vkFreeMemory(device_, mem_block.device_memory, nullptr);
            }
        }
    }
}

std::optional<uint32_t> Allocator::FindMemoryTypeIndex(
    const VkMemoryRequirements& memory_req,
    VkMemoryPropertyFlags flags) const {

    for (const auto& memory_type : memory_types_) {
        uint32_t type_bit = 1u << memory_type.memory_type_index;
        if ((memory_req.memoryTypeBits & type_bit) &&
            (memory_type.memory_properties & flags) == flags) {
            return static_cast<uint32_t>(memory_type.memory_type_index);
        }
    }

    return std::nullopt;
}

// 其他方法实现会在完整版本中添加...

} // namespace tekki
