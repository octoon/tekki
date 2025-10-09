#include "tekki/backend/vulkan/buffer.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace tekki::backend::vulkan {

uint64_t Buffer::DeviceAddress(const std::shared_ptr<Device>& device) {
    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = Raw;
    return vkGetBufferDeviceAddress(device->Raw, &addressInfo);
}

BufferDesc::BufferDesc() = default;

BufferDesc::BufferDesc(size_t size, VkBufferUsageFlags usage, MemoryLocation memoryLocation, std::optional<uint64_t> alignment)
    : Size(size), Usage(usage), MemoryLocation(memoryLocation), Alignment(alignment) {}

BufferDesc BufferDesc::NewGpuOnly(size_t size, VkBufferUsageFlags usage) {
    return BufferDesc(size, usage, MemoryLocation::GpuOnly, std::nullopt);
}

BufferDesc BufferDesc::NewCpuToGpu(size_t size, VkBufferUsageFlags usage) {
    return BufferDesc(size, usage, MemoryLocation::CpuToGpu, std::nullopt);
}

BufferDesc BufferDesc::NewGpuToCpu(size_t size, VkBufferUsageFlags usage) {
    return BufferDesc(size, usage, MemoryLocation::GpuToCpu, std::nullopt);
}

BufferDesc BufferDesc::WithAlignment(uint64_t alignment) {
    Alignment = alignment;
    return *this;
}

std::shared_ptr<Buffer> Device::CreateBufferImpl(
    VkDevice raw,
    gpu_allocator::VulkanAllocator& allocator,
    const BufferDesc& desc,
    const std::string& name) {
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.Size;
    bufferInfo.usage = desc.Usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(raw, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(raw, buffer, &requirements);

    if (desc.Alignment.has_value()) {
        requirements.alignment = std::max(requirements.alignment, desc.Alignment.value());
    }

    // TODO: why does `get_buffer_memory_requirements` fail to get the correct alignment on AMD?
    if (desc.Usage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) {
        // TODO: query device props
        requirements.alignment = std::max(requirements.alignment, 64ULL);
    }

    AllocationCreateDesc allocationDesc = {};
    allocationDesc.name = name.c_str();
    allocationDesc.requirements = requirements;
    allocationDesc.location = desc.MemoryLocation;
    allocationDesc.linear = true; // Buffers are always linear

    auto allocation = allocator.Allocate(allocationDesc);
    if (!allocation.HasValue()) {
        vkDestroyBuffer(raw, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(raw, buffer, allocation.Value()->Memory(), allocation.Value()->Offset()) != VK_SUCCESS) {
        vkDestroyBuffer(raw, buffer, nullptr);
        allocator.Free(*allocation.Value());
        throw std::runtime_error("Failed to bind buffer memory");
    }

    auto result = std::make_shared<Buffer>();
    result->Raw = buffer;
    result->Desc = desc;
    result->Allocation = *allocation.Value();
    return result;
}

std::shared_ptr<Buffer> Device::CreateBuffer(
    BufferDesc desc,
    const std::string& name,
    const std::vector<uint8_t>& initialData) {
    
    if (!initialData.empty()) {
        desc.Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    auto buffer = CreateBufferImpl(Raw, *GlobalAllocator, desc, name);

    if (!initialData.empty()) {
        auto scratchDesc = BufferDesc::NewCpuToGpu(desc.Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        auto scratchBuffer = CreateBufferImpl(Raw, *GlobalAllocator, scratchDesc, "Initial data for " + name);

        auto mappedData = scratchBuffer->Allocation.MappedSlice();
        if (mappedData.HasValue()) {
            std::copy(initialData.begin(), initialData.end(), mappedData.Value()->begin());
        }

        WithSetupCommandBuffer([&](VkCommandBuffer commandBuffer) {
            VkBufferCopy copyRegion = {};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = desc.Size;
            vkCmdCopyBuffer(commandBuffer, scratchBuffer->Raw, buffer->Raw, 1, &copyRegion);
        });
    }

    return buffer;
}

void Device::ImmediateDestroyBuffer(const std::shared_ptr<Buffer>& buffer) {
    vkDestroyBuffer(Raw, buffer->Raw, nullptr);
    if (!GlobalAllocator->Free(buffer->Allocation)) {
        throw std::runtime_error("Failed to deallocate buffer memory");
    }
}

template<typename F>
void Device::WithSetupCommandBuffer(F&& func) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    try {
        func(commandBuffer);
        EndSingleTimeCommands(commandBuffer);
    } catch (...) {
        EndSingleTimeCommands(commandBuffer);
        throw;
    }
}

VkCommandBuffer Device::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(Raw, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    return commandBuffer;
}

void Device::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(Raw, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence");
    }

    if (vkQueueSubmit(Queue, 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer");
    }

    if (vkWaitForFences(Raw, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fence");
    }

    vkDestroyFence(Raw, fence, nullptr);
    vkFreeCommandBuffers(Raw, CommandPool, 1, &commandBuffer);
}

} // namespace tekki::backend::vulkan