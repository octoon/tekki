#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_profiler/backend/ash/VulkanProfilerFrame.hpp"
#include "tekki/core/Result.hpp"

namespace tekki::backend::vulkan {

class ProfilerBuffer {
public:
    ProfilerBuffer(VkBuffer buffer, std::shared_ptr<void> allocation)
        : buffer_(buffer), allocation_(allocation) {}

    const uint8_t* MappedSlice() const {
        // Implementation depends on allocation mapping
        return static_cast<const uint8_t*>(allocation_.get());
    }

    VkBuffer Raw() const {
        return buffer_;
    }

private:
    VkBuffer buffer_;
    std::shared_ptr<void> allocation_;
};

class ProfilerBackend {
public:
    ProfilerBackend(VkDevice device, std::shared_ptr<void> allocator, float timestampPeriod)
        : device_(device), allocator_(allocator), timestampPeriod_(timestampPeriod) {}

    static ProfilerBackend Create(VkDevice device, std::shared_ptr<void> allocator, float timestampPeriod) {
        return ProfilerBackend(device, allocator, timestampPeriod);
    }

    ProfilerBuffer CreateQueryResultBuffer(size_t bytes) {
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

        // Memory allocation implementation depends on allocator interface
        // This is a placeholder - actual implementation would use the allocator
        auto allocation = std::make_shared<uint8_t[]>(bytes);
        
        // Bind memory to buffer
        // Implementation depends on specific memory binding requirements
        
        return ProfilerBuffer(buffer, allocation);
    }

    float TimestampPeriod() const {
        return timestampPeriod_;
    }

private:
    VkDevice device_;
    std::shared_ptr<void> allocator_;
    float timestampPeriod_;
};

using VkProfilerData = gpu_profiler::backend::ash::VulkanProfilerFrame<ProfilerBuffer>;

} // namespace tekki::backend::vulkan