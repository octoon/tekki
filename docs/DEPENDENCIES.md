# Rust 到 C++ 的依赖映射

本文档详细列出了 kajiya 使用的 Rust crate 以及它们在 tekki 中的 C++ 对应方案。

## 核心依赖

### Vulkan 与图形

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `ash` | Vulkan C++ | `vulkan-headers`, `vulkan-loader` | 使用 Vulkan C++ API 或原生 C API |
| `ash-window` | GLFW/SDL2 | `glfw` | 创建窗口与表面 |
| `gpu-allocator` | VulkanMemoryAllocator (VMA) | `vulkan-memory-allocator` | GPU 内存管理 |
| `vk-sync` | 手写屏障 | - | 直接翻译为 Vulkan 同步原语 |

### 数学与数值

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `glam` | GLM | `glm` | 图形数学库 |
| `half` | half | `half` | 半精度浮点数 |

### 着色器编译与反射

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `hassle-rs` | DirectXShaderCompiler | - | 直接链接 DXC |
| `rspirv` | SPIRV-Tools | `spirv-tools` | SPIR-V 操作工具集 |
| `rspirv-reflect` | SPIRV-Cross / SPIRV-Reflect | `spirv-cross` | 着色器反射 |
| `shader-prepper` | 自研或 DXC | - | 可能需要移植 |

### 图像处理

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `image` | stb_image | `stb` | 图像加载（PNG、JPG 等） |
| `exr` | OpenEXR | `openexr` | EXR 格式支持 |

### 资源加载

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `gltf` | TinyGLTF | `tinygltf` | glTF 2.0 资源加载 |

### 序列化

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `serde` | nlohmann/json + yaml-cpp | `nlohmann_json`, `yaml-cpp` | JSON/YAML 序列化 |
| `ron` | 自定义解析器 | - | RON 格式，可能需要移植 |
| `toml` | toml++ | `tomlplusplus` | TOML 解析 |
| `nanoserde` | 手写序列化 | - | 轻量级序列化逻辑 |

### 图形界面

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `imgui` | Dear ImGui | `imgui` | 原生 C++ 库，Rust 仅封装绑定 |

### 工具类库

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `anyhow` | 异常或 `expected<T,E>` | - | 采用 C++ 异常或 C++23 expected |
| `thiserror` | 自定义异常 | - | 定义专用异常类型 |
| `log` | spdlog | `spdlog` | 日志 |
| `fern` | spdlog | `spdlog` | 带格式的日志 |
| `parking_lot` | `std::mutex`, `std::shared_mutex` | - | C++ 标准同步原语 |
| `lazy_static` | 函数内静态变量 | - | 使用 C++11 magic statics |
| `bytemuck` | `reinterpret_cast` | - | 类型重解释 |

### 文件与路径

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `memmap2` | mmap 或 boost::iostreams | - | 内存映射文件 |
| `normpath` | `std::filesystem` | - | 路径归一化 |
| `relative-path` | `std::filesystem` | - | 相对路径处理 |

### 异步与并行

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `smol` | 自研或 Boost.Asio | `boost`（可选） | 异步运行时，可能需要移植 |
| `futures` | C++20 协程或回调 | - | Future/Promise 模式 |
| `easy-parallel` | `std::thread` 或线程池 | - | 基础并行执行 |
| `turbosloth` | **需要翻译** | - | 任务图系统，需要从 Rust 移植 |

### 专用库

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `blue-noise-sampler` | **需要翻译** | - | 蓝噪声生成，需要移植 |
| `radiant` | **需要翻译** | - | 辐射传输相关，实现需移植 |
| `gpu-profiler` | Tracy、Optick 或自研 | `tracy`（可选） | GPU 性能分析 |
| `puffin` | 自研或 Tracy | `tracy`（可选） | 性能分析工具 |

### 数据结构

| Rust Crate | C++ 库 | Conan 包 | 说明 |
|------------|--------|----------|------|
| `arrayvec` | `std::array` 或自定义 | - | 固定容量数组 |
| `byte-slice-cast` | `reinterpret_cast` | - | 字节切片转换 |
| `bytes` | `std::vector<uint8_t>` 或自定义 | - | 字节缓冲区 |

### 构建期依赖

| Rust Crate | C++ 对应 | 说明 |
|------------|----------|------|
| `derive_builder` | 手写实现或宏 | Builder 模式需手动实现 |

### 可选：DLSS 支持

| Rust Crate | C++ 库 | 说明 |
|------------|--------|------|
| `ngx_dlss` | NVIDIA NGX SDK | 直接链接 DLSS 库 |
| `wchar` | `<cwchar>` | 宽字符支持 |

### 开发期依赖

| Rust Crate | C++ 对应 | Conan 包 | 说明 |
|------------|----------|----------|------|
| `structopt` | CLI11 | `cli11` | 命令行解析 |
| `backtrace` | backward-cpp | - | 栈回溯 |
| `hotwatch` | efsw | `efsw` | 文件监控 |

## 需要翻译的库

下列 Rust crate 缺少直接的 C++ 替代方案，需要移植：

1. **turbosloth** —— 任务调度与缓存系统
   - 路径：`kajiya/crates/lib/turbosloth`（外部依赖）
   - 优先级：高
   - 复杂度：中

2. **blue-noise-sampler** —— 蓝噪声生成
   - 用于渲染采样模式
   - 优先级：高
   - 复杂度：低到中

3. **shader-prepper** —— 着色器预处理
   - 可尝试改用 DXC 的相关特性
   - 优先级：中
   - 复杂度：中

4. **自研的 kajiya crate** —— 所有内部 crate 也需要翻译：
   - `kajiya-backend`
   - `kajiya-rg`
   - `kajiya-asset`
   - `kajiya-asset-pipe`
   - `kajiya-imgui`
   - `kajiya-simple`
   - `kajiya`（主 crate）
   - `rust-shaders-shared`
   - `rust-shaders`（着色器代码）
   - `ash-imgui`

## 着色器翻译

`rust-shaders/` 目录内的 Rust 着色器需要重点关注：

- 原始形式：基于 Rust-GPU 的着色器
- 目标形式：HLSL 或 GLSL
- 策略：可手动翻译，或直接使用 `assets/shaders/` 中已有的 HLSL 实现

## 建议

1. **优先采用**：成熟的 C++ 库与稳定的 Conan 包
2. **必要时翻译**：kajiya 特定的自研 crate
3. **尽量延后**：复杂的异步系统，可先用简单同步方案过渡
4. **充分复用**：`assets/shaders/` 中的 HLSL 着色器可直接利用
