#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <memory>

namespace tekki {
namespace gpu_profiler {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 4;

// ScopeId: Identifies a profiling scope uniquely by frame and scope index
struct ScopeId {
    uint32_t frame;
    uint32_t scope;

    static ScopeId Invalid() {
        return ScopeId{~0u, ~0u};
    }

    uint64_t AsU64() const {
        return (static_cast<uint64_t>(frame) << 32) | static_cast<uint64_t>(scope);
    }

    static ScopeId FromU64(uint64_t val) {
        return ScopeId{
            static_cast<uint32_t>(val >> 32),
            static_cast<uint32_t>(val & 0xFFFFFFFF)
        };
    }

    bool operator==(const ScopeId& other) const {
        return frame == other.frame && scope == other.scope;
    }

    bool operator!=(const ScopeId& other) const {
        return !(*this == other);
    }

    bool operator<(const ScopeId& other) const {
        if (frame != other.frame) return frame < other.frame;
        return scope < other.scope;
    }
};

// NanoSecond: Represents a duration in nanoseconds
struct NanoSecond {
    uint64_t value;

    static NanoSecond FromRawNs(uint64_t ns) {
        return NanoSecond{ns};
    }

    uint64_t RawNs() const {
        return value;
    }

    double Ms() const {
        return static_cast<double>(value) / 1'000'000.0;
    }

    bool operator==(const NanoSecond& other) const {
        return value == other.value;
    }
};

// Internal structures
struct Scope {
    std::string name;
};

enum class FrameState {
    Invalid,
    Begin,
    End,
    Reported
};

struct Frame {
    FrameState state = FrameState::Invalid;
    uint32_t index = 0;
    std::vector<Scope> scopes;
};

// TimedScope: A profiling scope with its measured duration
struct TimedScope {
    std::string name;
    NanoSecond duration;
};

// TimedFrame: Contains all timed scopes for a frame
struct TimedFrame {
    std::vector<TimedScope> scopes;

    // Optional: Send timing data to puffin profiler (if integrated)
    // void SendToPuffin(uint64_t gpu_frame_start_ns) const;
};

// GpuProfiler: Main profiler class managing frame-based GPU timing
class GpuProfiler {
public:
    GpuProfiler();
    ~GpuProfiler() = default;

    // Frame management
    void BeginFrame();
    void EndFrame();

    // Scope management
    ScopeId CreateScope(const std::string& name);

    // Report collected timing data
    template<typename Iterator>
    void ReportDurations(Iterator begin, Iterator end) {
        if (begin == end) {
            last_report_.reset();
            return;
        }

        auto first = *begin;
        uint32_t first_scope_frame_idx = first.first.frame;
        size_t frame_count = frames_.size();

        Frame& frame = frames_[first_scope_frame_idx % frame_count];

        // Validate frame state
        if (frame.state != FrameState::End) {
            // throw std::runtime_error("report_durations called before end_frame or twice");
            return;
        }
        if (frame.index != first_scope_frame_idx) {
            // throw std::runtime_error("report_durations frame index mismatch");
            return;
        }

        frame.state = FrameState::Reported;

        // Build timed frame
        TimedFrame timed_frame;
        for (auto it = begin; it != end; ++it) {
            const auto& [scope_id, duration] = *it;

            if (scope_id.frame != first_scope_frame_idx) {
                // throw std::runtime_error("All scopes must be from the same frame");
                continue;
            }

            TimedScope timed_scope;
            timed_scope.name = std::move(frame.scopes[scope_id.scope].name);
            timed_scope.duration = duration;
            timed_frame.scopes.push_back(std::move(timed_scope));
        }

        last_report_ = std::move(timed_frame);
    }

    // Access last profiling report
    const std::optional<TimedFrame>& LastReport() const {
        return last_report_;
    }

    std::optional<TimedFrame> TakeLastReport() {
        return std::move(last_report_);
    }

private:
    Frame& CurrentFrame() {
        return frames_[frame_idx_ % frames_.size()];
    }

    std::vector<Frame> frames_;
    uint32_t frame_idx_ = 0;
    std::optional<TimedFrame> last_report_;
};

// Global profiler singleton
GpuProfiler& Profiler();

} // namespace gpu_profiler
} // namespace tekki
