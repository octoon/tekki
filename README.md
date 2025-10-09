# 💡 Tekki

> English version: `README_EN.md`

**kajiya 的 C++ 移植版——使用 Vulkan 的实验性实时全局光照渲染器**

tekki 是 [kajiya](https://github.com/EmbarkStudios/kajiya) 渲染器的完整 C++ 翻译版本，原项目由 Embark Studios 使用 Rust 编写。

## 关于项目

tekki 致力于一比一复刻 kajiya 的实验性实时全局光照渲染器，在保持架构与渲染技术一致的基础上，充分利用 C++ 生态。

仓库中的 `kajiya/` 目录保存了原始的 Rust 代码，保持原样以便随时对照；C++ 子系统按照上游项目的模块布局进行拆分，从而让翻译工作可追踪且易于比对。

### 原版 kajiya 的特性

- 混合渲染：光栅化、计算着色器与光线追踪协同
- 动态全局光照
  - 几何与光照完全动态，无需预计算
  - 体积化、时间递归的辐照度缓存，实现“无限”次反弹
  - 光线追踪的漫反射最终采样，捕获高频细节
  - 光线追踪的镜面反射，首跳后回退至漫反射
- 太阳光光线追踪软阴影
- 标准 PBR（GGX、粗糙度/金属度模型）
  - 能量守恒的多次散射 BRDF
- 参考级路径追踪模式
- 时域超分与抗锯齿
- 自然的色调映射
- 基于物理的眩光
- 基础运动模糊
- 自适应对比度锐化
- 可选 DLSS 支持
- glTF 网格加载（暂不含动画）
- 渲染图驱动整条管线

## 主要运行平台

硬件：
- NVIDIA RTX 系列
- NVIDIA GTX 1060 或更新型号（需至少 6 GB 显存）
- AMD Radeon RX 6000 系列

操作系统：
- Windows
- Linux

## 依赖

本项目使用 Conan 2 管理 C++ 依赖。

Rust crate 与 C++ 对应项（包含仍需直接翻译的部分）记录在 `docs/DEPENDENCIES.md`。

### Rust 到 C++ 的依赖映射

| Rust Crate | C++ 替代库 | 用途 |
|------------|------------|------|
| ash | vulkan-hpp / Vulkan C API | Vulkan 绑定 |
| glam | glm | 数学库 |
| image | stb_image | 图像加载 |
| gpu-allocator | VulkanMemoryAllocator (VMA) | GPU 内存管理 |
| rspirv/rspirv-reflect | SPIRV-Cross / SPIRV-Reflect | SPIR-V 反射 |
| hassle-rs | DirectXShaderCompiler | DXC 着色器编译 |
| winit | GLFW / SDL2 | 窗口系统 |
| imgui | Dear ImGui | 即时模式 GUI |
| serde | nlohmann/json, yaml-cpp | 序列化 |
| half | half | 半精度浮点 |
| exr | OpenEXR | EXR 图像 |
| parking_lot | std::mutex, std::shared_mutex | 线程同步 |

### 系统依赖

#### Linux
- `uuid-dev`
- Vulkan SDK
- DirectX Shader Compiler (DXC)

#### macOS
- `ossp-uuid`（通过 `brew install ossp-uuid`）
- 通过 MoltenVK 安装 Vulkan SDK

## 构建

### 前置条件

1. 安装 [Conan 2](https://conan.io/)：
```bash
pip install conan
```

2. 安装 [CMake](https://cmake.org/)（3.20 或更高版本）

3. 安装 [Vulkan SDK](https://vulkan.lunarg.com/)

### 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 使用 Conan 安装依赖
conan install .. --output-folder=. --build=missing

# 配置 CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . --config Release

# 运行
./bin/tekki-view
```

若在持续翻译过程中开发调试，请启用 `-DCMAKE_BUILD_TYPE=Debug` 并设置 `TEKKI_WITH_VALIDATION=ON`，以保留 kajiya 在开发模式下大量的验证流程。

### 运行测试

tekki 使用 Catch2 框架进行单元测试。测试默认启用，可通过 CMake 选项控制：

```bash
# 构建并运行所有测试
cd build
ctest --output-on-failure

# 或使用自定义目标
cmake --build . --target run-all-tests

# 禁用测试构建
cmake .. -DTEKKI_BUILD_TESTS=OFF
```

详细的测试文档请参见 `docs/TESTING.md`。

## 使用说明

```bash
./tekki-view [OPTIONS]
```

使用 `--help` 查看完整的命令行参数列表。

### 控制方式

- WSAD、QE：移动
- 鼠标 + 右键：旋转相机
- 鼠标 + 左键：旋转太阳
- Shift：加速移动
- Ctrl：减速移动
- 空格：切换到参考路径追踪
- Tab：显示/隐藏 UI

## 文档

- `docs/TRANSLATION.md` —— 翻译进度与指南，列出原始 Rust 文件对应关系。
- `docs/TESTING.md` —— 单元测试指南，包含测试运行和编写说明。
- `docs/DEPENDENCIES.md` —— Rust crate 与 C++ 依赖映射及说明。
- `docs/DEVELOPMENT.md` —— 开发环境配置、代码规范与仓库结构。

## 许可证

tekki 采用 [MIT](LICENSE-MIT) 与 [Apache 2.0](LICENSE-APACHE) 双许可证，与上游 kajiya 项目保持一致。详情参见 [LICENSE](LICENSE)。

## 资源加载

tekki 支持 [glTF 2.0](https://github.com/KhronosGroup/glTF) 格式的网格。将 `.gltf` 或 `.glb` 文件拖拽到窗口即可加载。

使用 `.exr` 或 `.hdr` 的图像环境贴图也可以通过拖拽方式加载。

## 项目结构

```
tekki/
├── kajiya/              # 原始 Rust 源码（供参考）
├── src/
│   ├── core/           # 核心工具模块
│   ├── backend/        # Vulkan 后端实现
│   ├── rg/             # 渲染图系统
│   ├── asset/          # 资源加载与管理
│   ├── renderer/       # 主渲染器实现
│   └── viewer/         # 查看器应用
├── shaders/            # HLSL/GLSL 着色器
├── assets/             # 示例资源
```
