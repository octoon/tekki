# Tekki Project Memory

## Project Overview

**tekki** is a C++ port of the [kajiya](https://github.com/EmbarkStudios/kajiya) renderer, originally written in Rust by Embark Studios. It's an experimental real-time global illumination renderer using Vulkan.

kajiya的源码见./kajiya目录。

## Key Project Information

### Build System & Dependencies
- **Build System**: CMake + Conan 2
- **C++ Standard**: C++20
- **Primary Dependencies**:
  - Vulkan SDK, GLFW, ImGui
  - GLM (math), spdlog (logging)
  - VMA (GPU memory allocation)
  - TinyGLTF (asset loading)
  - OpenEXR, stb_image (image formats)

### Project Structure
```
tekki/
    kajiya/             # Original Rust source (reference)
    src/                # C++ implementation
        core/           # Core utilities
        backend/        # Vulkan backend
        render_graph/   # Render graph system
        assets/         # Asset loading
        renderer/       # Main renderer
        app/            # Application layer
    shaders/            # HLSL/GLSL shaders
    tools/              # CLI tools
    docs/               # Documentation
```

### Translation Status
This is a work-in-progress translation from Rust to C++. The project follows a detailed translation plan documented in `docs/tekki_translation_plan.md`.

### Current Phase: Infrastructure (M1)
- CMake + Conan setup
- Core module skeleton
- Vulkan backend implementation

## Important Commands

### Build Commands
```bash
# Install dependencies
conan install . --output-folder=build --build=missing

# Configure and build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build build --config Release
```

### Development Commands
```bash
# Debug build with validation
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DTEKKI_WITH_VALIDATION=ON
```

## Key Technical Decisions

1. **Shader Language**: HLSL + DXC for SPIR-V generation
2. **Platform Layer**: GLFW + ImGui backend
3. **Memory Management**: VMA for GPU memory allocation
4. **Serialization**: JSON/TOML/YAML (replacing Rust's RON)
5. **Task System**: To be determined (replacing Rust's async_executor)

## Module Mapping (Rust to C++)

| Rust Crate | C++ Subsystem | Status |
|------------|---------------|--------|
| kajiya | src/renderer/world | � |
| kajiya-backend | src/backend | � |
| kajiya-rg | src/render_graph | � |
| kajiya-asset | src/assets | � |
| kajiya-simple | src/app/simple_loop | � |

## Development Notes

- The project maintains the original Rust source in `kajiya/` for reference
- Translation follows the original module structure for easy comparison
- Focus on maintaining the same rendering architecture and techniques
- Use `-DTEKKI_WITH_VALIDATION=ON` during development to enable Vulkan validation layers

## Documentation

- `docs/tekki_translation_plan.md` - Detailed translation roadmap
- `docs/DEPENDENCIES.md` - Dependency mapping
- `docs/DEVELOPMENT.md` - Development guidelines
- `docs/TRANSLATION.md` - Translation progress tracker

## License
Dual licensed under MIT and Apache 2.0, matching the upstream kajiya project.