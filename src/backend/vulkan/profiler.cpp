#include "tekki/backend/vulkan/profiler.h"
#include <stdexcept>
#include <memory>
#include <cstdint>

namespace tekki::backend::vulkan {

ProfilerBuffer::ProfilerBuffer(VkBuffer buffer, std::shared_ptr<void> allocation)
    : buffer_(buffer), allocation_(allocation) {}

const uint8_t* ProfilerBuffer::MappedSlice() const {
    return static_cast<const uint8_t*>(allocation_.get());
}

VkBuffer ProfilerBuffer::Raw() const {
    return buffer_;
}

ProfilerBackend::ProfilerBackend(VkDevice device, std::shared_ptr<void> allocator, float timestampPeriod)
    : device_(device), allocator_(allocator), timestampPeriod_(timestampPeriod) {}

ProfilerBackend ProfilerBackend::Create(VkDevice device, std::shared_ptr<void> allocator, float timestampPeriod) {
    return ProfilerBackend(device, allocator, timestampPeriod);
}

ProfilerBuffer ProfilerBackend::CreateQueryResultBuffer(size_t bytes) {
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

    auto allocation = std::make_shared<uint8_t[]>(bytes);
    
    return ProfilerBuffer(buffer, allocation);
}

float ProfilerBackend::TimestampPeriod() const {
    return timestampPeriod_;
}

} // namespace tekki::backend::vulkan