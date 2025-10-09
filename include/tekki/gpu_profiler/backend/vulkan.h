#pragma once

#include "tekki/gpu_profiler/gpu_profiler.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <memory>

namespace tekki {

// Vulkan Buffer 接口
class VulkanBuffer {
public:
    virtual ~VulkanBuffer() = default;

    virtual const uint8_t* MappedSlice() const = 0;
    virtual VkBuffer Raw() const = 0;
};

// Vulkan Backend 接口
class VulkanBackend {
public:
    virtual ~VulkanBackend() = default;

    virtual std::unique_ptr<VulkanBuffer> CreateQueryResultBuffer(size_t bytes) = 0;
    virtual float TimestampPeriod() const = 0;
};

// Vulkan Active Scope
struct VulkanActiveScope {
    uint32_t query_id;
};

// Vulkan Profiler Frame
class VulkanProfilerFrame {
public:
    VulkanProfilerFrame(VkDevice device, VulkanBackend& backend);
    ~VulkanProfilerFrame();

    // 禁止拷贝
    VulkanProfilerFrame(const VulkanProfilerFrame&) = delete;
    VulkanProfilerFrame& operator=(const VulkanProfilerFrame&) = delete;

    // 允许移动
    VulkanProfilerFrame(VulkanProfilerFrame&&) noexcept;
    VulkanProfilerFrame& operator=(VulkanProfilerFrame&&) noexcept;

    // 开始一个 scope
    VulkanActiveScope BeginScope(VkDevice device, VkCommandBuffer cb, ScopeId scope_id);

    // 结束一个 scope
    void EndScope(VkDevice device, VkCommandBuffer cb, const VulkanActiveScope& active_scope);

    // 帧开始前调用
    void BeginFrame(VkDevice device, VkCommandBuffer cmd);

    // 帧结束后调用
    void EndFrame(VkDevice device, VkCommandBuffer cmd);

private:
    static constexpr size_t MAX_QUERY_COUNT = 1024;
    using DurationRange = std::array<uint64_t, 2>;

    void ReportDurations();
    std::vector<std::pair<ScopeId, DurationRange>> RetrievePreviousResults();

    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    std::unique_ptr<VulkanBuffer> buffer_;
    std::atomic<uint32_t> next_query_idx_{0};
    std::unique_ptr<std::atomic<uint64_t>[]> query_scope_ids_;
    float timestamp_period_ = 1.0f;
};

} // namespace tekki
