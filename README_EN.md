> 中文版：`README.md`

# 💡 Tekki

**C++ port of kajiya - Experimental real-time global illumination renderer using Vulkan**

This is a complete C++ translation of the [kajiya](https://github.com/EmbarkStudios/kajiya) renderer, originally written in Rust by Embark Studios.

## About

tekki aims to provide a one-to-one translation of kajiya's experimental real-time global illumination renderer to C++, maintaining the same architecture and rendering techniques while leveraging the C++ ecosystem.

The original Rust sources from Embark Studios live in `kajiya/` and remain untouched so you can diff the translation side-by-side. Each C++ subsystem mirrors the module layout of the upstream project to keep the porting effort traceable.

### Original kajiya Features

* Hybrid rendering using a mixture of raster, compute, and ray-tracing
* Dynamic global illumination
  * Fully dynamic geometry and lighting without precomputation
  * Volumetric temporally-recurrent irradiance cache for "infinite" bounces
  * Ray-traced diffuse final gather for high-frequency details
  * Ray-traced specular, falling back to diffuse after the first hit
* Sun with ray-traced soft shadows
* Standard PBR with GGX and roughness/metalness
  * Energy-preserving multi-scattering BRDF
* Reference path-tracing mode
* Temporal super-resolution and anti-aliasing
* Natural tone mapping
* Physically-based glare
* Basic motion blur
* Contrast-adaptive sharpening
* Optional DLSS support
* glTF mesh loading (no animations yet)
* A render graph running it all

## Primary platforms

Hardware:
* Nvidia RTX series
* Nvidia GTX 1060 and newer _with 6+ GB of VRAM_
* AMD Radeon RX 6000 series

Operating systems:
* Windows
* Linux

## Dependencies

This project uses Conan 2 for C++ dependency management.

The mapping between Rust crates and their C++ counterparts (including the cases where we still translate Rust code directly) is documented in detail in `docs/DEPENDENCIES.md`.

### Rust to C++ Dependency Mapping

| Rust Crate | C++ Alternative | Purpose |
|------------|-----------------|---------|
| ash | vulkan-hpp / Vulkan C API | Vulkan bindings |
| glam | glm | Mathematics library |
| image | stb_image | Image loading |
| gpu-allocator | VulkanMemoryAllocator (VMA) | GPU memory management |
| rspirv/rspirv-reflect | SPIRV-Cross / SPIRV-Reflect | SPIR-V reflection |
| hassle-rs | DirectXShaderCompiler | DXC shader compiler |
| winit | GLFW / SDL2 | Windowing |
| imgui | Dear ImGui | Immediate mode GUI |
| serde | nlohmann/json, yaml-cpp | Serialization |
| half | half | Half-precision float |
| exr | OpenEXR | EXR image format |
| parking_lot | std::mutex, std::shared_mutex | Thread synchronization |

### System Dependencies

#### Linux
* `uuid-dev`
* Vulkan SDK
* DirectX Shader Compiler (DXC)

#### macOS
* `ossp-uuid` (`brew install ossp-uuid`)
* Vulkan SDK via MoltenVK

## Building

### Prerequisites

1. Install [Conan 2](https://conan.io/):
```bash
pip install conan
```

2. Install [CMake](https://cmake.org/) (3.20 or higher)

3. Install [Vulkan SDK](https://vulkan.lunarg.com/)

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Install dependencies with Conan
conan install .. --output-folder=. --build=missing

# Configure CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run
./bin/tekki-view
```

For incremental translation work, enable `-DCMAKE_BUILD_TYPE=Debug` and `TEKKI_WITH_VALIDATION=ON` to retain the behaviour of kajiya's validation-heavy development builds.

## Usage

```bash
./tekki-view [OPTIONS]
```

For a list of supported command-line switches see `--help`.

### Controls

* WSAD, QE - movement
* Mouse + RMB - rotate the camera
* Mouse + LMB - rotate the sun
* Shift - move faster
* Ctrl - move slower
* Space - switch to reference path tracing
* Tab - show/hide the UI

## Documentation

- `docs/TRANSLATION.md` – progress tracker and translation guidelines referencing the original Rust files.
- `docs/DEPENDENCIES.md` – Rust crate to C++ dependency mapping and notes.
- `docs/DEVELOPMENT.md` – development environment setup, coding standards, and repository layout.

## License

tekki is dual licensed under the [MIT](LICENSE-MIT) and [Apache 2.0](LICENSE-APACHE) licenses, matching the upstream kajiya project. See [LICENSE](LICENSE) for details.

## Loading assets

tekki supports meshes in the [glTF 2.0](https://github.com/KhronosGroup/glTF) format. Simply drag-n-drop the `.gltf` or `.glb` file onto the window.

Image-based lights (`.exr` or `.hdr` files) can also be loaded via drag-n-drop.

## Project Structure

```
tekki/
├── kajiya/              # Original Rust source (for reference)
├── src/
│   ├── core/           # Core utilities and base classes
│   ├── backend/        # Vulkan backend
│   ├── rg/             # Render graph
│   ├── asset/          # Asset loading and management
│   ├── renderer/       # Main renderer implementation
│   └── viewer/         # Viewer application
├── shaders/            # HLSL/GLSL shaders
├── assets/             # Sample assets
├── CMakeLists.txt
├── conanfile.py
└── README.md
```

## Translation Status

This project is in early development. Check the issues for translation progress of individual modules.

## Acknowledgments

This is a port of [kajiya](https://github.com/EmbarkStudios/kajiya) by Embark Studios. All credit for the original rendering techniques and architecture goes to the original authors.

Special thanks to:
* The kajiya team at Embark Studios
* Felix Westin for [MinimalAtmosphere](https://github.com/Fewes/MinimalAtmosphere)
* AMD for the FidelityFX Shadow Denoiser
