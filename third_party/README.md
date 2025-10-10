# Third-Party Dependencies

This directory contains ports and adaptations of external libraries used by the Tekki renderer.

## Libraries

### gpu_allocator
A C++ port of the Rust [gpu-allocator](https://github.com/Traverse-Research/gpu-allocator) crate.
Provides efficient Vulkan memory allocation with support for:
- Dedicated block allocation for large resources
- Free-list based allocation for small resources
- Memory type selection and management
- Allocation statistics and debugging

**Original License**: MIT or Apache-2.0

### gpu_profiler
A C++ port of the Rust [gpu-profiler](https://github.com/Traverse-Research/gpu-profiler) crate.
Provides GPU performance profiling capabilities:
- GPU timestamp queries
- Hierarchical scope profiling
- Backend abstraction (Vulkan support)
- Performance metrics collection

**Original License**: MIT or Apache-2.0

## Structure

Each library follows a consistent structure:
```
library_name/
├── CMakeLists.txt    # Build configuration
├── include/          # Public headers
│   └── tekki/
│       └── library_name/
└── src/              # Implementation files
    └── ...
```

## Building

These libraries are built automatically as part of the main Tekki build process.
They are configured as static libraries and linked into the main backend.

## Attribution

These libraries are ports of open-source Rust crates developed by Traverse Research and other contributors.
We maintain the original licensing terms and provide attribution to the original authors.
