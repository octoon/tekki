#include "tekki/gpu_profiler/vulkan_profiler.h"
#include <algorithm>
#include <cassert>

namespace tekki {
namespace gpu_profiler {

VulkanProfilerFrame::VulkanProfilerFrame(VkDevice device, VulkanBackend& backend)
    : device_(device)
    , timestamp_period_(backend.TimestampPeriod())
{
    // Create buffer for query results
    buffer_ = backend.CreateQueryResultBuffer(MAX_QUERY_COUNT * sizeof(DurationRange));

    // Create query pool
    VkQueryPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = MAX_QUERY_COUNT * 2; // 2 timestamps per query (begin/end)

    VkResult result = vkCreateQueryPool(device, &pool_info, nullptr, &query_pool_);
    assert(result == VK_SUCCESS && "Failed to create query pool");

    // Initialize query scope IDs array
    query_scope_ids_ = std::make_unique<std::atomic<uint64_t>[]>(MAX_QUERY_COUNT);
    for (size_t i = 0; i < MAX_QUERY_COUNT; ++i) {
        query_scope_ids_[i].store(ScopeId::Invalid().AsU64(), std::memory_order_relaxed);
    }
}

VulkanProfilerFrame::~VulkanProfilerFrame() {
    if (query_pool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device_, query_pool_, nullptr);
    }
}

VulkanActiveScope VulkanProfilerFrame::BeginScope(
    VkDevice /*device*/,
    VkCommandBuffer cb,
    ScopeId scope_id)
{
    uint32_t query_id = next_query_idx_.fetch_add(1, std::memory_order_relaxed);

    query_scope_ids_[query_id].store(scope_id.AsU64(), std::memory_order_relaxed);

    vkCmdWriteTimestamp(
        cb,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        query_pool_,
        query_id * 2
    );

    return VulkanActiveScope{query_id};
}

void VulkanProfilerFrame::EndScope(
    VkDevice /*device*/,
    VkCommandBuffer cb,
    const VulkanActiveScope& active_scope)
{
    vkCmdWriteTimestamp(
        cb,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        query_pool_,
        active_scope.query_id * 2 + 1
    );
}

void VulkanProfilerFrame::BeginFrame(VkDevice /*device*/, VkCommandBuffer cmd) {
    // Report previous frame's results
    ReportDurations();

    // Reset query pool
    vkCmdResetQueryPool(cmd, query_pool_, 0, MAX_QUERY_COUNT * 2);

    // Reset query counter
    next_query_idx_.store(0, std::memory_order_relaxed);
}

void VulkanProfilerFrame::EndFrame(VkDevice /*device*/, VkCommandBuffer cmd) {
    uint32_t valid_query_count = next_query_idx_.load(std::memory_order_relaxed);

    // Copy query results to buffer
    vkCmdCopyQueryPoolResults(
        cmd,
        query_pool_,
        0,                          // firstQuery
        valid_query_count * 2,      // queryCount
        buffer_->Raw(),             // dstBuffer
        0,                          // dstOffset
        8,                          // stride (8 bytes per timestamp)
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );
}

void VulkanProfilerFrame::ReportDurations() {
    auto previous_results = RetrievePreviousResults();
    double ns_per_tick = static_cast<double>(timestamp_period_);

    std::vector<std::pair<ScopeId, NanoSecond>> durations;
    durations.reserve(previous_results.size());

    for (const auto& [scope_id, duration_range] : previous_results) {
        uint64_t duration_ticks = 0;
        if (duration_range[1] >= duration_range[0]) {
            duration_ticks = duration_range[1] - duration_range[0];
        }

        NanoSecond duration = NanoSecond::FromRawNs(
            static_cast<uint64_t>(static_cast<double>(duration_ticks) * ns_per_tick)
        );

        durations.emplace_back(scope_id, duration);
    }

    Profiler().ReportDurations(durations.begin(), durations.end());
}

std::vector<std::pair<ScopeId, DurationRange>> VulkanProfilerFrame::RetrievePreviousResults() const {
    uint32_t valid_query_count = next_query_idx_.load(std::memory_order_relaxed);

    auto mapped_slice = buffer_->MappedSlice();

    assert(mapped_slice.size() % sizeof(DurationRange) == 0);
    assert(mapped_slice.size() / sizeof(DurationRange) >= valid_query_count);

    // Interpret buffer as array of DurationRanges
    const DurationRange* durations = reinterpret_cast<const DurationRange*>(mapped_slice.data());

    // Build result vector
    std::vector<std::pair<ScopeId, DurationRange>> result;
    result.reserve(valid_query_count);

    for (uint32_t i = 0; i < valid_query_count; ++i) {
        ScopeId scope_id = ScopeId::FromU64(
            query_scope_ids_[i].load(std::memory_order_relaxed)
        );
        result.emplace_back(scope_id, durations[i]);
    }

    // Sort by scope_id
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    return result;
}

} // namespace gpu_profiler
} // namespace tekki
