#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tekki {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 4;

// Forward declarations
class GpuProfiler;

// 获取全局 profiler 实例
GpuProfiler& Profiler();

// Scope ID - 用于标识一个性能测量 scope
struct ScopeId {
    uint32_t frame;
    uint32_t scope;

    static ScopeId Invalid();

    uint64_t AsU64() const;
    static ScopeId FromU64(uint64_t val);

    bool operator==(const ScopeId& other) const = default;
    bool operator!=(const ScopeId& other) const = default;
    bool operator<(const ScopeId& other) const;
};

// 纳秒时间单位
struct NanoSecond {
    uint64_t value;

    static NanoSecond FromRawNs(uint64_t ns);
    uint64_t RawNs() const;
    double Ms() const;

    bool operator==(const NanoSecond& other) const = default;
};

// 带计时信息的 Scope
struct TimedScope {
    std::string name;
    NanoSecond duration;
};

// 一帧的性能数据
struct TimedFrame {
    std::vector<TimedScope> scopes;

    // 发送性能数据到 puffin (可选功能)
    // void SendToPuffin(uint64_t gpu_frame_start_ns) const;
};

// GPU 性能分析器
class GpuProfiler {
public:
    GpuProfiler() = default;
    ~GpuProfiler() = default;

    // 开始一帧
    void BeginFrame();

    // 结束一帧
    void EndFrame();

    // 创建一个新的 scope
    ScopeId CreateScope(std::string name);

    // 报告 scope 的持续时间
    template<typename Iterator>
    void ReportDurations(Iterator begin, Iterator end);

    // 获取上一帧的报告
    const TimedFrame* LastReport() const;

    // 取出上一帧的报告 (移动语义)
    std::optional<TimedFrame> TakeLastReport();

private:
    enum class FrameState {
        Invalid,
        Begin,
        End,
        Reported
    };

    struct Scope {
        std::string name;
    };

    struct Frame {
        FrameState state = FrameState::Invalid;
        uint32_t index = 0;
        std::vector<Scope> scopes;
    };

    std::vector<Frame> frames_{MAX_FRAMES_IN_FLIGHT};
    uint32_t frame_idx_ = 0;
    std::optional<TimedFrame> last_report_;

    Frame& CurrentFrame();
};

// 模板实现
template<typename Iterator>
void GpuProfiler::ReportDurations(Iterator begin, Iterator end) {
    if (begin == end) {
        last_report_ = std::nullopt;
        return;
    }

    auto first = *begin;
    uint32_t first_scope_frame_idx = first.first.frame;
    size_t frame_count = frames_.size();

    Frame& frame = frames_[first_scope_frame_idx % frame_count];

    // 验证帧状态
    if (frame.state != FrameState::End || frame.index != first_scope_frame_idx) {
        // 错误:帧状态不正确
        return;
    }

    frame.state = FrameState::Reported;

    // 收集所有 timed scopes
    std::vector<TimedScope> timed_scopes;
    for (auto it = begin; it != end; ++it) {
        auto [scope_id, duration] = *it;

        // 验证所有 scope 都属于同一帧
        if (scope_id.frame != first_scope_frame_idx) {
            continue;
        }

        TimedScope timed_scope;
        timed_scope.name = std::move(frame.scopes[scope_id.scope].name);
        timed_scope.duration = duration;
        timed_scopes.push_back(std::move(timed_scope));
    }

    last_report_ = TimedFrame{std::move(timed_scopes)};
}

} // namespace tekki
