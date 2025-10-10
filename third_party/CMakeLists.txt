# Third-party dependencies
# This directory contains ports and adaptations of external libraries

# GPU Allocator - Vulkan memory allocation library
# Based on Rust's gpu-allocator crate
add_subdirectory(gpu_allocator)

# GPU Profiler - GPU performance profiling library
# Based on Rust's gpu-profiler crate
add_subdirectory(gpu_profiler)

# Hassle - DXC shader compiler wrapper
# Based on Rust's hassle-rs crate
add_subdirectory(hassle)
