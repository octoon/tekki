# rspirv-reflect C++ Translation

This is a C++ translation of the original Rust [rspirv-reflect](https://github.com/Traverse-Research/rspirv-reflect) library, which provides basic SPIR-V reflection capabilities to extract descriptor set information from SPIR-V bytecode.

## Features

- Extract descriptor sets and bindings from SPIR-V modules
- Determine compute shader workgroup sizes
- Get push constant range information
- Support for various descriptor types (buffers, images, samplers, etc.)
- Support for static arrays and bindless (unbounded) arrays

## Usage

### Basic Example

```cpp
#include "rspirv-reflect/reflection.h"
#include <vector>

using namespace rspirv_reflect;

// Load SPIR-V bytecode
std::vector<uint32_t> spirv_code = /* ... load from file ... */;

// Create reflection object
auto reflection_result = Reflection::NewFromSpirv(spirv_code);
if (IsErr(reflection_result)) {
    auto error = UnwrapErr(reflection_result);
    std::cerr << "Failed to create reflection: " << error.what() << std::endl;
    return;
}

auto& reflection = Unwrap(reflection_result);

// Get compute group size (if compute shader)
auto group_size = reflection.GetComputeGroupSize();
if (group_size) {
    auto [x, y, z] = *group_size;
    std::cout << "Compute group size: (" << x << ", " << y << ", " << z << ")" << std::endl;
}

// Get descriptor sets
auto sets_result = reflection.GetDescriptorSets();
if (IsErr(sets_result)) {
    auto error = UnwrapErr(sets_result);
    std::cerr << "Failed to get descriptor sets: " << error.what() << std::endl;
    return;
}

auto& sets = Unwrap(sets_result);
for (auto& [set_index, bindings] : sets) {
    std::cout << "Set " << set_index << ":" << std::endl;
    for (auto& [binding_index, info] : bindings) {
        std::cout << "  Binding " << binding_index << ": "
                  << info.name << " (" << info.ty.ToString() << ")" << std::endl;
    }
}

// Get push constant range
auto push_constants_result = reflection.GetPushConstantRange();
if (IsErr(push_constants_result)) {
    auto error = UnwrapErr(push_constants_result);
    std::cerr << "Failed to get push constants: " << error.what() << std::endl;
    return;
}

auto& push_constants = Unwrap(push_constants_result);
if (push_constants) {
    std::cout << "Push constants: offset=" << push_constants->offset
              << ", size=" << push_constants->size << std::endl;
}
```

## Building

This library uses CMake and depends on [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross).

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## API Reference

### Reflection Class

- `static Result<Reflection> NewFromSpirv(const std::vector<uint32_t>& spirv)`
- `std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> GetComputeGroupSize() const`
- `Result<std::map<uint32_t, std::map<uint32_t, DescriptorInfo>>> GetDescriptorSets()`
- `Result<std::optional<PushConstantInfo>> GetPushConstantRange()`
- `std::string Disassemble() const`

### Descriptor Types

Supported descriptor types:
- `SAMPLER`, `COMBINED_IMAGE_SAMPLER`, `SAMPLED_IMAGE`, `STORAGE_IMAGE`
- `UNIFORM_TEXEL_BUFFER`, `STORAGE_TEXEL_BUFFER`
- `UNIFORM_BUFFER`, `STORAGE_BUFFER`
- `UNIFORM_BUFFER_DYNAMIC`, `STORAGE_BUFFER_DYNAMIC`
- `INPUT_ATTACHMENT`
- `INLINE_UNIFORM_BLOCK_EXT`, `ACCELERATION_STRUCTURE_KHR`, `ACCELERATION_STRUCTURE_NV`

### Binding Count Types

- `One()` - Single resource binding
- `StaticSized(size_t)` - Fixed-size array of bindings
- `Unbounded()` - Variable-sized (bindless) array

## Error Handling

The library uses a `Result<T>` type that can contain either a value of type `T` or a `ReflectError`. Use `IsOk()` and `IsErr()` to check the result, and `Unwrap()` to get the value (throws on error).

## Differences from Rust Version

- Uses SPIRV-Cross instead of rspirv for SPIR-V parsing
- C++ error handling with exceptions and `Result<T>` type
- Similar API but adapted to C++ idioms
- Same functionality for descriptor set reflection

## License

MIT OR Apache-2.0, same as the original Rust library.