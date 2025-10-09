# GPU Allocator C++ Translation

This directory contains the C++ translation of the Rust `gpu-allocator` library (version 0.28.0).

## Overview

The gpu-allocator is a memory allocator for GPU memory in Vulkan. It provides:
- Sub-allocation strategies to minimize the number of actual allocations
- Free list allocator for general-purpose sub-allocations
- Dedicated block allocator for large or special allocations
- Support for CPU-mapped memory
- Memory leak detection and reporting

## Translation Status

✅ **Complete** - The core Vulkan allocator has been fully translated.

### Translated Components

1. **Core Types** (`gpu_allocator.h`):
   - `MemoryLocation` - Memory location hints (GpuOnly, CpuToGpu, GpuToCpu)
   - `AllocationSizes` - Configuration for memory block sizes
   - `AllocatorDebugSettings` - Debug and logging settings
   - `AllocationScheme` - Dedicated vs managed allocations
   - `Allocation` - Represents a single memory allocation
   - `Allocator` - Main allocator class

2. **Sub-Allocators** (`allocator_internal.h`, `allocator_base.cpp`):
   - `DedicatedBlockAllocator` - For dedicated/large allocations
   - `FreeListAllocator` - For general-purpose sub-allocations with free list management
   - Support for buffer/image granularity and alignment

3. **Vulkan Integration** (`gpu_allocator.cpp`):
   - Full Vulkan memory type and heap management
   - Automatic memory type selection based on location
   - Support for dedicated allocations (buffer/image)
   - Memory mapping support
   - Memory leak reporting on shutdown

## API Differences from Rust

### Error Handling
- **Rust**: `Result<T, AllocationError>`
- **C++**: `std::variant<T, AllocationError>` with helper functions `IsOk()`, `IsErr()`, `Unwrap()`

### Ownership
- **Rust**: Ownership via move semantics and `Box`/`Arc`
- **C++**: Smart pointers (`std::unique_ptr`, `std::shared_ptr`)

### Collections
- **Rust**: `HashMap`, `HashSet`
- **C++**: `std::unordered_map`, `std::unordered_set`

## Usage Example

```cpp
#include <tekki/gpu_allocator/gpu_allocator.h>

using namespace tekki::gpu_allocator;

// Create allocator
AllocatorCreateDesc desc;
desc.Instance = vk_instance;
desc.Device = vk_device;
desc.PhysicalDevice = vk_physical_device;
desc.BufferDeviceAddress = true;

auto allocator_result = Allocator::New(desc);
if (IsErr(allocator_result)) {
    // Handle error
    return;
}
auto allocator = Unwrap(allocator_result);

// Allocate buffer memory
AllocationCreateDesc alloc_desc;
alloc_desc.Name = "My Buffer";
alloc_desc.Requirements = vk_memory_requirements;
alloc_desc.Location = MemoryLocation::GpuOnly;
alloc_desc.Linear = true;
alloc_desc.Scheme = AllocationScheme::GpuAllocatorManaged;

auto allocation_result = allocator->Allocate(alloc_desc);
if (IsErr(allocation_result)) {
    // Handle error
    return;
}
auto allocation = Unwrap(allocation_result);

// Use allocation
vkBindBufferMemory(device, buffer, allocation.Memory(), allocation.Offset());

// Free allocation
allocator->Free(std::move(allocation));
```

## Integration with Tekki

The gpu-allocator has been integrated into the Tekki renderer:

1. Added to build system in `src/CMakeLists.txt`
2. Updated `include/tekki/backend/vulkan/device.h` to use `gpu_allocator::Allocator`
3. Updated `include/tekki/backend/vulkan/buffer.h` to use `gpu_allocator::Allocation`
4. Updated `include/tekki/backend/vulkan/image.h` includes
5. Removed dependency on the Rust-based VMA wrapper

## Build

The library is automatically built as part of the Tekki project:

```bash
./Setup.bat        # Windows Debug
./Setup.bat release # Windows Release
```

## Dependencies

- Vulkan SDK (for Vulkan API)
- spdlog (for logging)
- C++20 compiler

## License

Dual licensed under MIT OR Apache-2.0, matching the upstream gpu-allocator project.

## Notes

- This translation focuses on the Vulkan backend only (no D3D12 or Metal)
- Visualizer features are not included
- Backtrace/stack trace support is not included (simplified implementation)
- The translation maintains the same allocation strategies and performance characteristics as the Rust version
