#include "tekki/gpu_profiler/gpu_profiler.h"
#include <cassert>

namespace tekki {

// 全局 profiler 实例
GpuProfiler& Profiler() {
    static GpuProfiler instance;
    return instance;
}

// ScopeId 实现
ScopeId ScopeId::Invalid() {
    return ScopeId{UINT32_MAX, UINT32_MAX};
}

uint64_t ScopeId::AsU64() const {
    return (static_cast<uint64_t>(frame) << 32) | static_cast<uint64_t>(scope);
}

ScopeId ScopeId::FromU64(uint64_t val) {
    return ScopeId{
        static_cast<uint32_t>(val >> 32),
        static_cast<uint32_t>(val & UINT32_MAX)
    };
}

bool ScopeId::operator<(const ScopeId& other) const {
    if (frame != other.frame) {
        return frame < other.frame;
    }
    return scope < other.scope;
}

// NanoSecond 实现
NanoSecond NanoSecond::FromRawNs(uint64_t ns) {
    return NanoSecond{ns};
}

uint64_t NanoSecond::RawNs() const {
    return value;
}

double NanoSecond::Ms() const {
    return static_cast<double>(value) / 1'000'000.0;
}

// GpuProfiler 实现
void GpuProfiler::BeginFrame() {
    Frame& frame = CurrentFrame();

    assert(frame.state != FrameState::Begin && "begin_frame called twice");

    frame.state = FrameState::Begin;
    frame.index = frame_idx_;
    frame.scopes.clear();
}

void GpuProfiler::EndFrame() {
    Frame& frame = CurrentFrame();

    if (frame.state == FrameState::Invalid || frame.state == FrameState::Reported) {
        assert(false && "end_frame called without begin_frame");
        return;
    }

    if (frame.state == FrameState::End) {
        assert(false && "end_frame called twice");
        return;
    }

    frame.state = FrameState::End;
    frame_idx_++;
}

ScopeId GpuProfiler::CreateScope(std::string name) {
    Frame& frame = CurrentFrame();
    uint32_t next_scope_id = static_cast<uint32_t>(frame.scopes.size());

    frame.scopes.push_back(Scope{std::move(name)});

    return ScopeId{frame_idx_, next_scope_id};
}

const TimedFrame* GpuProfiler::LastReport() const {
    if (last_report_.has_value()) {
        return &last_report_.value();
    }
    return nullptr;
}

std::optional<TimedFrame> GpuProfiler::TakeLastReport() {
    return std::move(last_report_);
}

GpuProfiler::Frame& GpuProfiler::CurrentFrame() {
    size_t frame_count = frames_.size();
    return frames_[frame_idx_ % frame_count];
}

} // namespace tekki
