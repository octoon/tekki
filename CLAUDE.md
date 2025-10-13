# Tekki 项目备忘录

## 项目概述

**tekki** 是 [kajiya](https://github.com/EmbarkStudios/kajiya) 渲染器的 C++ 移植版本，原版由 Embark Studios 使用 Rust 编写。这是一个实验性的实时全局光照渲染器，使用 Vulkan 图形接口。

kajiya 的源码位于 ./kajiya 目录。

## 命名风格
文件名主要采用小写下划线风格。
类名使用大驼峰命名法，方法名和函数名也采用大驼峰命名法。

## 关键项目信息

### 构建系统与依赖
- **构建系统**: CMake + Conan 2
- **C++ 标准**: C++20
- **主要依赖**:
  - Vulkan SDK、GLFW、ImGui
  - GLM（数学库）、spdlog（日志）
  - VMA（GPU 内存分配）
  - TinyGLTF（资源加载）
  - OpenEXR、stb_image（图像格式）

### 项目结构
```
tekki/
    kajiya/             # 原始 Rust 源代码（参考）
    include/            # 公共头文件
        tekki/
            core/       # 核心工具
            backend/    # Vulkan 后端
            render_graph/ # 渲染图系统
            asset/      # 资源加载
            renderer/   # 主渲染器
            shared/     # 共享工具
    src/                # C++ 实现
        core/           # 核心实现
        backend/        # Vulkan 后端实现
        render_graph/   # 渲染图实现
        renderer/       # 渲染器实现
        viewer/         # 查看器应用
    shaders/            # HLSL/GLSL 着色器
    conan/              # Conan 配置和配置文件
        profiles/
    docs/               # 文档
```

### 翻译状态
这是一个从 Rust 到 C++ 的进行中的翻译项目。项目遵循 `docs/tekki_translation_plan.md` 中记录的详细翻译计划。

### 当前阶段：渲染器实现（M2）
- 核心基础设施完成（M1）
- 渲染图系统已实现
- 渲染器模块进行中（IBL、IR 缓存、光照等）
- 查看器应用功能正常

## 重要命令

### 构建命令
```bash
# Windows(bash) - 调试构建（推荐用于开发）
./Setup.bat

# Windows(bash) - 发布构建
./Setup.bat release

# 使用 Conan 配置文件的手动构建
conan install . --profile:host conan/profiles/windows-msvc-debug --profile:build conan/profiles/windows-msvc-debug --build=missing --output-folder=build/debug
cmake --preset conan-default
cmake --build --preset conan-debug --parallel
```

### 开发命令
```bash
# 带验证的调试构建（默认启用）
Setup.bat

# 运行查看器应用
build\debug\bin\tekki-view.exe

# 清理构建
rmdir /s build
del CMakeUserPresets.json
```

## 关键技术决策

1. **着色器语言**: HLSL + DXC 用于 SPIR-V 生成
2. **平台层**: GLFW + ImGui 后端
3. **内存管理**: VMA 用于 GPU 内存分配
4. **序列化**: JSON/TOML/YAML（替代 Rust 的 RON）
5. **任务系统**: 待确定（替代 Rust 的 async_executor）

## 模块映射（Rust 到 C++）

| Rust 包 | C++ 子系统 | 状态 |
|---------|------------|------|
| kajiya-backend | src/backend | ✅ |
| kajiya-rg | src/render_graph | ✅ |
| kajiya-asset | src/asset | ✅ |
| kajiya | src/renderer | 🚧 |
| kajiya-simple | src/viewer | ✅ |

## 开发说明

- 项目在 `kajiya/` 目录中保留了原始 Rust 源代码供参考
- 翻译遵循原始模块结构以便于比较
- 重点保持相同的渲染架构和技术
- 开发时使用 `-DTEKKI_WITH_VALIDATION=ON` 启用 Vulkan 验证层

## 文档

- `docs/tekki_translation_plan.md` - 详细翻译路线图
- `docs/DEPENDENCIES.md` - 依赖映射
- `docs/DEVELOPMENT.md` - 开发指南
- `docs/TRANSLATION.md` - 翻译进度跟踪

## 许可证
采用 MIT 和 Apache 2.0 双重许可，与上游 kajiya 项目保持一致。