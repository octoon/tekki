#pragma once

// Main GPU Profiler API
// This is a C++ translation of the gpu-profiler Rust library
// License: MIT OR Apache-2.0

#include "tekki/gpu_profiler/gpu_profiler.h"
#include "tekki/gpu_profiler/vulkan_profiler.h"

namespace tekki {
namespace gpu_profiler {

// Usage example:
//
// // Setup (per swapchain image):
// VulkanBackend backend_impl = ...;
// VulkanProfilerFrame profiler_frames[num_swapchain_images] = {
//     VulkanProfilerFrame(device, backend_impl),
//     VulkanProfilerFrame(device, backend_impl),
//     ...
// };
//
// // Each frame:
// auto& profiler_frame = profiler_frames[current_frame_index];
//
// // At the start of command buffer recording:
// Profiler().BeginFrame();
// profiler_frame.BeginFrame(device, cmd);
//
// // Around GPU work:
// ScopeId scope_id = Profiler().CreateScope("MyGpuWork");
// auto active_scope = profiler_frame.BeginScope(device, cmd, scope_id);
// // ... record GPU commands ...
// profiler_frame.EndScope(device, cmd, active_scope);
//
// // At the end of command buffer recording:
// profiler_frame.EndFrame(device, cmd);
// Profiler().EndFrame();
//
// // Later, after GPU work completes:
// if (auto report = Profiler().LastReport()) {
//     for (const auto& scope : report->scopes) {
//         printf("%s: %.2f ms\n", scope.name.c_str(), scope.duration.Ms());
//     }
// }

} // namespace gpu_profiler
} // namespace tekki
