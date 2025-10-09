#include "tekki/gpu_profiler/gpu_profiler.h"
#include <cassert>

namespace tekki {
namespace gpu_profiler {

GpuProfiler::GpuProfiler()
    : frames_(MAX_FRAMES_IN_FLIGHT)
    , frame_idx_(0)
{
}

void GpuProfiler::BeginFrame() {
    Frame& frame = CurrentFrame();

    // Assert not already in Begin state
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

ScopeId GpuProfiler::CreateScope(const std::string& name) {
    Frame& frame = CurrentFrame();
    uint32_t next_scope_id = static_cast<uint32_t>(frame.scopes.size());

    frame.scopes.push_back(Scope{name});

    return ScopeId{frame_idx_, next_scope_id};
}

// Global profiler singleton
GpuProfiler& Profiler() {
    static GpuProfiler instance;
    return instance;
}

} // namespace gpu_profiler
} // namespace tekki
