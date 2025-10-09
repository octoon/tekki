#pragma once

#include "tekki/gpu_profiler/gpu_profiler.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <array>
#include <span>

namespace tekki {
namespace gpu_profiler {

constexpr size_t MAX_QUERY_COUNT = 1024;
using DurationRange = std::array<uint64_t, 2>;

// VulkanBuffer: Abstraction for a buffer that can store query results
class VulkanBuffer {
public:
    virtual ~VulkanBuffer() = default;
    virtual std::span<const uint8_t> MappedSlice() const = 0;
    virtual VkBuffer Raw() const = 0;
};

// VulkanBackend: Abstraction for creating buffers and getting device properties
class VulkanBackend {
public:
    virtual ~VulkanBackend() = default;
    virtual std::unique_ptr<VulkanBuffer> CreateQueryResultBuffer(size_t bytes) = 0;
    virtual float TimestampPeriod() const = 0;
};

// VulkanActiveScope: RAII handle for an active profiling scope
struct VulkanActiveScope {
    uint32_t query_id;
};

// VulkanProfilerFrame: Manages GPU profiling for a single frame
class VulkanProfilerFrame {
public:
    VulkanProfilerFrame(VkDevice device, VulkanBackend& backend);
    ~VulkanProfilerFrame();

    // Disable copy, allow move
    VulkanProfilerFrame(const VulkanProfilerFrame&) = delete;
    VulkanProfilerFrame& operator=(const VulkanProfilerFrame&) = delete;
    VulkanProfilerFrame(VulkanProfilerFrame&&) = default;
    VulkanProfilerFrame& operator=(VulkanProfilerFrame&&) = default;

    // Begin a profiling scope
    VulkanActiveScope BeginScope(VkDevice device, VkCommandBuffer cb, ScopeId scope_id);

    // End a profiling scope
    void EndScope(VkDevice device, VkCommandBuffer cb, const VulkanActiveScope& active_scope);

    // Call before recording any profiling scopes in the frame
    void BeginFrame(VkDevice device, VkCommandBuffer cmd);

    // Call after recording all profiling scopes in the frame
    void EndFrame(VkDevice device, VkCommandBuffer cmd);

    VkQueryPool GetQueryPool() const { return query_pool_; }

private:
    void ReportDurations();
    std::vector<std::pair<ScopeId, DurationRange>> RetrievePreviousResults() const;

    std::unique_ptr<VulkanBuffer> buffer_;
    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    std::atomic<uint32_t> next_query_idx_{0};
    std::unique_ptr<std::atomic<uint64_t>[]> query_scope_ids_;
    float timestamp_period_ = 1.0f;
};

} // namespace gpu_profiler
} // namespace tekki
