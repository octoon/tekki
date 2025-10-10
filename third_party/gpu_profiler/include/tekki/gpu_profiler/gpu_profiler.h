#pragma once

// Main GPU Profiler API
// This is a C++ translation of the gpu-profiler Rust library
// License: MIT OR Apache-2.0

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tekki {

// Scope identifier combining frame and scope indices
struct ScopeId {
    uint32_t frame;
    uint32_t scope;

    static ScopeId Invalid();
    uint64_t AsU64() const;
    static ScopeId FromU64(uint64_t val);
    bool operator<(const ScopeId& other) const;
    bool operator==(const ScopeId& other) const {
        return frame == other.frame && scope == other.scope;
    }
};

// Nanosecond duration
struct NanoSecond {
    uint64_t value;

    static NanoSecond FromRawNs(uint64_t ns);
    uint64_t RawNs() const;
    double Ms() const;
};

// A named profiling scope
struct Scope {
    std::string name;
};

// A scope with timing information
struct TimedScope {
    std::string name;
    NanoSecond duration;
};

// Frame with timing information
struct TimedFrame {
    std::vector<TimedScope> scopes;
};

// Frame state
enum class FrameState {
    Invalid,
    Begin,
    End,
    Reported
};

// GPU Profiler - manages profiling scopes and timing reports
class GpuProfiler {
public:
    GpuProfiler() {
        // Initialize with 3 frames for triple buffering
        frames_.resize(3);
    }

    void BeginFrame();
    void EndFrame();
    ScopeId CreateScope(std::string name);

    const TimedFrame* LastReport() const;
    std::optional<TimedFrame> TakeLastReport();

    // Called by backend implementations to report timing results
    template<typename Iterator>
    void ReportDurations(Iterator begin, Iterator end) {
        std::vector<std::pair<ScopeId, NanoSecond>> timings(begin, end);
        if (!timings.empty() && !frames_.empty()) {
            // Get the frame index from the first timing
            uint32_t frame_idx = timings[0].first.frame;
            ReportTimings(frame_idx, timings);
        }
    }

    // Called by backend implementations to report timing results
    void ReportTimings(uint32_t frame_idx, const std::vector<std::pair<ScopeId, NanoSecond>>& timings) {
        // Find frame
        for (auto& frame : frames_) {
            if (frame.index == frame_idx && frame.state == FrameState::End) {
                TimedFrame timed_frame;
                for (const auto& [scope_id, duration] : timings) {
                    if (scope_id.scope < frame.scopes.size()) {
                        timed_frame.scopes.push_back(TimedScope{
                            frame.scopes[scope_id.scope].name,
                            duration
                        });
                    }
                }
                last_report_ = std::move(timed_frame);
                frame.state = FrameState::Reported;
                break;
            }
        }
    }

private:
    struct Frame {
        FrameState state = FrameState::Invalid;
        uint32_t index = 0;
        std::vector<Scope> scopes;
    };

    Frame& CurrentFrame();

    std::vector<Frame> frames_;
    uint32_t frame_idx_ = 0;
    std::optional<TimedFrame> last_report_;
};

// Global profiler accessor
GpuProfiler& Profiler();

} // namespace tekki
