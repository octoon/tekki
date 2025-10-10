#include "tekki/backend/vulkan/profiler.h"
#include <stdexcept>
#include <memory>
#include <cstdint>

namespace tekki::backend::vulkan {

std::unique_ptr<tekki::VulkanBuffer> ProfilerBackend::CreateQueryResultBuffer(size_t bytes) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(bytes);
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);

    // 使用 allocator 分配内存
    tekki::AllocationCreateDesc allocDesc{};
    allocDesc.name = "profiler_query_buffer";
    allocDesc.requirements = requirements;
    allocDesc.location = tekki::MemoryLocation::GpuToCpu;
    allocDesc.linear = true; // Buffers are always linear

    auto allocation = allocator_->Allocate(allocDesc);
    if (!allocation) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    // 绑定内存到 buffer
    result = vkBindBufferMemory(device_, buffer, allocation.value().Memory(), allocation.value().Offset());
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("Failed to bind buffer memory");
    }

    return std::make_unique<ProfilerBuffer>(buffer, std::move(allocation.value()));
}

} // namespace tekki::backend::vulkan
