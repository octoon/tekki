#include "tekki/gpu_profiler/backend/vulkan.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace tekki {

VulkanProfilerFrame::VulkanProfilerFrame(VkDevice device, VulkanBackend& backend)
    : timestamp_period_(backend.TimestampPeriod()) {

    // 创建查询结果缓冲区
    buffer_ = backend.CreateQueryResultBuffer(MAX_QUERY_COUNT * sizeof(DurationRange));

    // 创建查询池
    VkQueryPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = MAX_QUERY_COUNT * 2;

    VkResult result = vkCreateQueryPool(device, &pool_info, nullptr, &query_pool_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create query pool");
    }

    // 初始化 scope ID 数组
    query_scope_ids_ = std::make_unique<std::atomic<uint64_t>[]>(MAX_QUERY_COUNT);
    for (size_t i = 0; i < MAX_QUERY_COUNT; ++i) {
        query_scope_ids_[i].store(ScopeId::Invalid().AsU64(), std::memory_order_relaxed);
    }
}

VulkanProfilerFrame::~VulkanProfilerFrame() {
    // 析构函数会在移动后的对象上调用,需要检查
    // query_pool_ 和 buffer_ 会在移动后被设置为 null
}

VulkanProfilerFrame::VulkanProfilerFrame(VulkanProfilerFrame&& other) noexcept
    : query_pool_(other.query_pool_),
      buffer_(std::move(other.buffer_)),
      next_query_idx_(other.next_query_idx_.load()),
      query_scope_ids_(std::move(other.query_scope_ids_)),
      timestamp_period_(other.timestamp_period_) {
    other.query_pool_ = VK_NULL_HANDLE;
}

VulkanProfilerFrame& VulkanProfilerFrame::operator=(VulkanProfilerFrame&& other) noexcept {
    if (this != &other) {
        query_pool_ = other.query_pool_;
        buffer_ = std::move(other.buffer_);
        next_query_idx_.store(other.next_query_idx_.load());
        query_scope_ids_ = std::move(other.query_scope_ids_);
        timestamp_period_ = other.timestamp_period_;

        other.query_pool_ = VK_NULL_HANDLE;
    }
    return *this;
}

VulkanActiveScope VulkanProfilerFrame::BeginScope(
    VkDevice /*device*/,
    VkCommandBuffer cb,
    ScopeId scope_id) {

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
    const VulkanActiveScope& active_scope) {

    vkCmdWriteTimestamp(
        cb,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        query_pool_,
        active_scope.query_id * 2 + 1
    );
}

void VulkanProfilerFrame::BeginFrame(VkDevice /*device*/, VkCommandBuffer cmd) {
    ReportDurations();

    vkCmdResetQueryPool(cmd, query_pool_, 0, MAX_QUERY_COUNT * 2);

    next_query_idx_.store(0, std::memory_order_relaxed);
}

void VulkanProfilerFrame::EndFrame(VkDevice /*device*/, VkCommandBuffer cmd) {
    uint32_t valid_query_count = next_query_idx_.load(std::memory_order_relaxed);

    vkCmdCopyQueryPoolResults(
        cmd,
        query_pool_,
        0,
        valid_query_count * 2,
        buffer_->Raw(),
        0,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );
}

void VulkanProfilerFrame::ReportDurations() {
    auto previous_results = RetrievePreviousResults();
    double ns_per_tick = static_cast<double>(timestamp_period_);

    std::vector<std::pair<ScopeId, NanoSecond>> durations;
    durations.reserve(previous_results.size());

    for (const auto& [scope_id, duration_range] : previous_results) {
        uint64_t duration_ticks = duration_range[1] > duration_range[0]
            ? duration_range[1] - duration_range[0]
            : 0;

        uint64_t duration_ns = static_cast<uint64_t>(
            static_cast<double>(duration_ticks) * ns_per_tick
        );

        durations.emplace_back(scope_id, NanoSecond::FromRawNs(duration_ns));
    }

    Profiler().ReportDurations(durations.begin(), durations.end());
}

std::vector<std::pair<ScopeId, VulkanProfilerFrame::DurationRange>>
VulkanProfilerFrame::RetrievePreviousResults() {
    uint32_t valid_query_count = next_query_idx_.load(std::memory_order_relaxed);

    const uint8_t* mapped_slice = buffer_->MappedSlice();
    const DurationRange* durations = reinterpret_cast<const DurationRange*>(mapped_slice);

    std::vector<std::pair<ScopeId, DurationRange>> result;
    result.reserve(valid_query_count);

    for (uint32_t i = 0; i < valid_query_count; ++i) {
        uint64_t scope_id_u64 = query_scope_ids_[i].load(std::memory_order_relaxed);
        ScopeId scope_id = ScopeId::FromU64(scope_id_u64);
        result.emplace_back(scope_id, durations[i]);
    }

    // 按 scope_id 排序
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    return result;
}

} // namespace tekki
