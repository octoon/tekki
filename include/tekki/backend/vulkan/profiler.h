#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "tekki/gpu_profiler/backend/vulkan.h"
#include "tekki/gpu_allocator/vulkan/allocator.h"
#include "tekki/core/result.h"

namespace tekki::backend::vulkan
{

// ProfilerBuffer 实现 VulkanBuffer 接口
class ProfilerBuffer : public tekki::VulkanBuffer {
public:
    ProfilerBuffer(VkBuffer buffer, tekki::Allocation allocation)
        : buffer_(buffer), allocation_(std::move(allocation)) {}

    const uint8_t* MappedSlice() const override {
        return allocation_.MappedSlice();
    }

    VkBuffer Raw() const override {
        return buffer_;
    }

private:
    VkBuffer buffer_;
    tekki::Allocation allocation_;
};

// ProfilerBackend 实现 VulkanBackend 接口
class ProfilerBackend : public tekki::VulkanBackend {
public:
    ProfilerBackend(VkDevice device, tekki::VulkanAllocator* allocator, float timestampPeriod)
        : device_(device), allocator_(allocator), timestampPeriod_(timestampPeriod) {}

    std::unique_ptr<tekki::VulkanBuffer> CreateQueryResultBuffer(size_t bytes) override;
    float TimestampPeriod() const override { return timestampPeriod_; }

private:
    VkDevice device_;
    tekki::VulkanAllocator* allocator_;
    float timestampPeriod_;
};

// VkProfilerData 对应 VulkanProfilerFrame
using VkProfilerData = tekki::VulkanProfilerFrame;

} // namespace tekki::backend::vulkan