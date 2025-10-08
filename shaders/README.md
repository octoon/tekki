# Shaders

This directory will contain the shader code for the tekki renderer.

## Organization

The original kajiya project has two types of shaders:

1. **HLSL Shaders** (in `kajiya/assets/shaders/`)
   - These are ready-to-use HLSL shaders
   - Can be copied and adapted for tekki

2. **Rust GPU Shaders** (in `kajiya/crates/lib/rust-shaders/`)
   - Written using the rust-gpu project
   - Need to be translated to HLSL or use the existing HLSL equivalents

## Translation Strategy

For the initial implementation, we will:
1. Copy the HLSL shaders from `kajiya/assets/shaders/`
2. Adapt them as needed for the C++ codebase
3. Later, consider translating or replacing Rust-GPU specific shaders

## Shader Compilation

- Shaders will be compiled using DirectX Shader Compiler (DXC)
- SPIR-V output for Vulkan compatibility
- Runtime compilation with shader hot-reloading support

## Directory Structure

```
shaders/
├── common/         # Common utilities and includes
├── rt/            # Ray tracing shaders
├── raster/        # Rasterization shaders
├── compute/       # Compute shaders
└── post/          # Post-processing shaders
```
