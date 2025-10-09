# GPU Profiler

C++ translation of the `gpu-profiler` Rust library for GPU timing measurement.

## Overview

This library provides frame-based GPU profiling using Vulkan timestamp queries. It allows you to measure the duration of GPU work across multiple frames while handling frame-in-flight synchronization.

## Key Features

- **Frame-based profiling**: Track GPU timing across multiple in-flight frames
- **Scope-based API**: Create named scopes to measure specific GPU operations
- **Vulkan backend**: Built on Vulkan timestamp queries
- **Minimal overhead**: Lightweight design with minimal CPU/GPU overhead
- **Safe synchronization**: Properly handles multiple frames in flight

## Architecture

The library is split into two main components:

1. **Core Profiler** (`gpu_profiler.h/cpp`):
   - Manages profiling scopes and timing reports
   - Handles frame lifecycle (begin/end)
   - Provides global profiler singleton

2. **Vulkan Backend** (`vulkan_profiler.h/cpp`):
   - Manages Vulkan query pools
   - Records GPU timestamps
   - Retrieves and processes query results

## Usage

### 1. Implement VulkanBackend

You need to provide an implementation of the `VulkanBackend` interface:

```cpp
class MyVulkanBackend : public tekki::gpu_profiler::VulkanBackend {
public:
    std::unique_ptr<VulkanBuffer> CreateQueryResultBuffer(size_t bytes) override {
        // Create a host-visible buffer for query results
        // The buffer must remain mapped
        return std::make_unique<MyVulkanBuffer>(device_, bytes);
    }

    float TimestampPeriod() const override {
        // Return the timestamp period from physical device properties
        return physical_device_properties_.limits.timestampPeriod;
    }
};
```

### 2. Create Profiler Frames

Create one `VulkanProfilerFrame` per swapchain image:

```cpp
MyVulkanBackend backend;
std::vector<tekki::gpu_profiler::VulkanProfilerFrame> profiler_frames;

for (size_t i = 0; i < swapchain_images.size(); ++i) {
    profiler_frames.emplace_back(device, backend);
}
```

### 3. Profile GPU Work

```cpp
using namespace tekki::gpu_profiler;

// Get current frame's profiler
auto& profiler_frame = profiler_frames[current_frame_index];

// Start frame profiling
Profiler().BeginFrame();
profiler_frame.BeginFrame(device, command_buffer);

// Create and record a profiling scope
ScopeId scope_id = Profiler().CreateScope("MyGpuWork");
auto active_scope = profiler_frame.BeginScope(device, command_buffer, scope_id);

// ... Record your GPU commands here ...
vkCmdDraw(...);

// End the scope
profiler_frame.EndScope(device, command_buffer, active_scope);

// End frame profiling
profiler_frame.EndFrame(device, command_buffer);
Profiler().EndFrame();

// Submit command buffer...
```

### 4. Retrieve Results

After GPU work completes (typically N frames later):

```cpp
if (auto report = Profiler().LastReport()) {
    for (const auto& scope : report->scopes) {
        printf("%s: %.2f ms\n", scope.name.c_str(), scope.duration.Ms());
    }
}
```

## Implementation Notes

### Frame Lifecycle

1. `BeginFrame()`: Resets profiling state for the current frame
2. Create scopes and record timestamps during command buffer recording
3. `EndFrame()`: Copies query results to CPU-visible buffer
4. Results are reported N frames later (based on MAX_FRAMES_IN_FLIGHT)

### Synchronization

The library assumes you have multiple frames in flight (typically 2-3). Query results from frame N are read back when frame N+MAX_FRAMES_IN_FLIGHT starts, ensuring GPU work has completed.

### Query Pool Size

The library supports up to `MAX_QUERY_COUNT` (1024) scopes per frame. Each scope uses 2 timestamp queries (begin/end).

## Differences from Rust Version

1. **No Puffin integration**: The `SendToPuffin` functionality is commented out as it requires the Puffin profiler library
2. **RAII Cleanup**: The C++ version uses destructors for cleanup instead of manual Drop impl
3. **No OpenGL backend**: Only Vulkan backend is implemented
4. **Standard library containers**: Uses `std::vector`, `std::string` instead of Rust equivalents
5. **Error handling**: Uses asserts instead of panics (can be changed to exceptions if needed)

## License

Dual licensed under MIT OR Apache-2.0, matching the original Rust library.

## Original Source

Translated from: https://github.com/EmbarkStudios/kajiya/tree/main/crates/lib/kajiya-gpu-profiler
