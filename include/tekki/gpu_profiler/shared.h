#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <iterator>
#include <algorithm>

namespace gpu_profiler {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 4;

// NanoSecond - timestamp in nanoseconds
struct NanoSecond {
    uint64_t Value;

    NanoSecond() : Value(0) {}
    explicit NanoSecond(uint64_t ns) : Value(ns) {}

    static NanoSecond FromRawNs(uint64_t ns) { return NanoSecond(ns); }
    uint64_t RawNs() const { return Value; }
    double Ms() const { return static_cast<double>(Value) / 1'000'000.0; }
};

// ScopeId - identifies a profiling scope within a frame
struct ScopeId {
    uint32_t Frame;
    uint32_t Scope;

    static ScopeId Invalid() { return ScopeId{UINT32_MAX, UINT32_MAX}; }

    uint64_t AsU64() const {
        return (static_cast<uint64_t>(Frame) << 32) | static_cast<uint64_t>(Scope);
    }

    static ScopeId FromU64(uint64_t val) {
        return ScopeId{
            static_cast<uint32_t>(val >> 32),
            static_cast<uint32_t>(val & UINT32_MAX)
        };
    }

    bool operator==(const ScopeId& other) const {
        return Frame == other.Frame && Scope == other.Scope;
    }

    bool operator!=(const ScopeId& other) const { return !(*this == other); }

    bool operator<(const ScopeId& other) const {
        if (Frame != other.Frame) return Frame < other.Frame;
        return Scope < other.Scope;
    }
};

// TimedScope - profiling scope with timing information
struct TimedScope {
    std::string Name;
    NanoSecond Duration;

    TimedScope() = default;
    TimedScope(std::string name, NanoSecond duration)
        : Name(std::move(name)), Duration(duration) {}
};

// TimedFrame - collection of timed scopes for a frame
struct TimedFrame {
    std::vector<TimedScope> Scopes;

    TimedFrame() = default;
};

// Internal scope structure
struct Scope {
    std::string Name;

    Scope() = default;
    explicit Scope(std::string name) : Name(std::move(name)) {}
};

// Frame state enum
enum class FrameState {
    Invalid,
    Begin,
    End,
    Reported,
};

// Frame structure
struct Frame {
    FrameState State;
    uint32_t Index;
    std::vector<Scope> Scopes;

    Frame() : State(FrameState::Invalid), Index(0) {}
};

// GpuProfiler - main profiler class
class GpuProfiler {
private:
    std::vector<Frame> frames_;
    uint32_t frame_idx_;
    std::optional<TimedFrame> last_report_;

    Frame& GetCurrentFrame() {
        return frames_[frame_idx_ % frames_.size()];
    }

public:
    GpuProfiler() : frame_idx_(0) {
        frames_.resize(MAX_FRAMES_IN_FLIGHT);
    }

    void BeginFrame() {
        Frame& frame = GetCurrentFrame();

        if (frame.State == FrameState::Begin) {
            throw std::runtime_error("begin_frame called twice");
        }

        frame.State = FrameState::Begin;
        frame.Index = frame_idx_;
        frame.Scopes.clear();
    }

    void EndFrame() {
        Frame& frame = GetCurrentFrame();

        switch (frame.State) {
            case FrameState::Invalid:
            case FrameState::Reported:
                throw std::runtime_error("end_frame called without begin_frame");
            case FrameState::Begin:
                frame.State = FrameState::End;
                break;
            case FrameState::End:
                throw std::runtime_error("end_frame called twice");
        }

        frame_idx_++;
    }

    ScopeId CreateScope(std::string name) {
        Frame& frame = GetCurrentFrame();
        uint32_t next_scope_id = static_cast<uint32_t>(frame.Scopes.size());

        frame.Scopes.emplace_back(std::move(name));

        return ScopeId{frame_idx_, next_scope_id};
    }

    template<typename Iterator>
    void ReportDurations(Iterator begin, Iterator end) {
        if (begin == end) {
            last_report_ = std::nullopt;
            return;
        }

        auto first = *begin;
        ScopeId first_scope_id = first.first;
        uint32_t first_scope_frame_idx = first_scope_id.Frame;
        size_t frame_count = frames_.size();

        Frame& frame = frames_[first_scope_frame_idx % frame_count];

        // Validate frame state
        if (frame.State == FrameState::End && frame.Index == first_scope_frame_idx) {
            frame.State = FrameState::Reported;
        } else if (frame.State == FrameState::Reported) {
            throw std::runtime_error("report_durations called twice");
        } else {
            throw std::runtime_error("report_durations called before end_frame");
        }

        // Build timed frame
        TimedFrame timed_frame;

        for (auto it = begin; it != end; ++it) {
            ScopeId scope_id = it->first;
            NanoSecond duration = it->second;

            if (scope_id.Frame != first_scope_frame_idx) {
                throw std::runtime_error("Scope from different frame");
            }

            size_t scope_index = scope_id.Scope;
            if (scope_index >= frame.Scopes.size()) {
                throw std::runtime_error("Invalid scope index");
            }

            timed_frame.Scopes.emplace_back(
                std::move(frame.Scopes[scope_index].Name),
                duration
            );
        }

        last_report_ = std::move(timed_frame);
    }

    const std::optional<TimedFrame>& LastReport() const {
        return last_report_;
    }

    std::optional<TimedFrame> TakeLastReport() {
        auto report = std::move(last_report_);
        last_report_ = std::nullopt;
        return report;
    }
};

// Global profiler instance (thread-safe)
inline std::mutex& GetProfilerMutex() {
    static std::mutex mutex;
    return mutex;
}

inline GpuProfiler& GetGlobalProfiler() {
    static GpuProfiler profiler;
    return profiler;
}

// Helper to lock and get the profiler
class ProfilerLock {
private:
    std::unique_lock<std::mutex> lock_;
    GpuProfiler& profiler_;

public:
    ProfilerLock() : lock_(GetProfilerMutex()), profiler_(GetGlobalProfiler()) {}

    GpuProfiler& Get() { return profiler_; }
    GpuProfiler* operator->() { return &profiler_; }
};

} // namespace gpu_profiler
