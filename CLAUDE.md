# Tekki Project Memory

## Project Overview

**tekki** is a C++ port of the [kajiya](https://github.com/EmbarkStudios/kajiya) renderer, originally written in Rust by Embark Studios. It's an experimental real-time global illumination renderer using Vulkan.

kajiya的源码见./kajiya目录。


## 命名风格
文件名主要以小写下划线风格为主。
类名采用大驼峰，方法名和函数名也采用大驼峰。

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
    include/            # Public headers
        tekki/
            core/       # Core utilities
            backend/    # Vulkan backend
            render_graph/ # Render graph system
            asset/      # Asset loading
            renderer/   # Main renderer
            shared/     # Shared utilities
    src/                # C++ implementation
        core/           # Core implementation
        backend/        # Vulkan backend implementation
        render_graph/   # Render graph implementation
        renderer/       # Renderer implementation
        viewer/         # Viewer application
    shaders/            # HLSL/GLSL shaders
    conan/              # Conan profiles and configuration
        profiles/
    docs/               # Documentation
```

### Translation Status
This is a work-in-progress translation from Rust to C++. The project follows a detailed translation plan documented in `docs/tekki_translation_plan.md`.

### Current Phase: Renderer Implementation (M2)
- Core infrastructure complete (M1)
- Render graph system implemented
- Renderer modules in progress (IBL, IR cache, lighting, etc.)
- Viewer application functional

## Important Commands

### Build Commands
```bash
# Windows(bash) - Debug build (recommended for development)
./Setup.bat

# Windows(bash) - Release build
./Setup.bat release

# Manual build with Conan profiles
conan install . --profile:host conan/profiles/windows-msvc-debug --profile:build conan/profiles/windows-msvc-debug --build=missing --output-folder=build/debug
cmake --preset conan-default
cmake --build --preset conan-debug --parallel
```

### Development Commands
```bash
# Debug build with validation (enabled by default)
Setup.bat

# Run the viewer application
build\debug\bin\tekki-view.exe

# Clean build
rmdir /s build
del CMakeUserPresets.json
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
| kajiya-backend | src/backend | ✅ |
| kajiya-rg | src/render_graph | ✅ |
| kajiya-asset | src/asset | ✅ |
| kajiya | src/renderer | 🚧 |
| kajiya-simple | src/viewer | ✅ |

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