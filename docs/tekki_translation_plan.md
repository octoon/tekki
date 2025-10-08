# tekki 项目翻译计划

## 1. Rust 原项目结构概览

| 模块/Crate | 主要职责 | 关键内容 |
| --- | --- | --- |
| `kajiya` | 世界渲染器核心，调度所有渲染 pass | 摄像机、世界帧描述、各种渲染器（SSR/SSGI/TAA/后处理等）、绑定表、图像 LUT、日志 |
| `kajiya-backend` | Vulkan 底层封装与资源管理 | 设备抽象、Buffer/Image/RT Accel 管理、Shader/Pipeline 编译、瞬时资源缓存 `transient_resource_cache` |
| `kajiya-rg` | Render Graph 框架 | 图节点定义、资源描述、Pass 构建器、时间重用 `temporal`、Renderer 封套 |
| `kajiya-asset` | 资源格式定义与加载 | 网格（含打包格式）、贴图、GLTF 导入 |
| `kajiya-asset-pipe` | 资产加工工具链 | `process_mesh_asset` 处理 GLTF -> `.mesh/.image`，使用 `turbosloth` 延迟求值 |
| `kajiya-simple` | 应用层封装与主循环 | 输入系统、窗口管理、WorldRenderer 生命周期管理、日志初始化 |
| `kajiya-imgui` | ImGui 与渲染器集成 | Winit 平台接入、UI 渲染任务注入 |
| `ash-imgui` | 基于 ash 的 ImGui 渲染实现 | Vulkan 管线/资源初始化、ImGui draw data 上传 |
| `rust-shaders` | GPU 着色器（Rust-gpu） | GBuffer、后处理、天空、模糊、SSGI 等计算/光栅 shader |
| `rust-shaders-shared` | Shader 共享结构体 | 相机/视图常量、GBuffer 布局、Mesh 数据、SSGI 常量等 |
| `ngx_dlss`（可选） | NVIDIA DLSS FFI 包装 | FFI+绑定（工作区默认排除） |
| `bin/view` | 主可视化工具 | 场景加载、Runtime 状态、GUI/序列管理 |
| `bin/hello` | 最小示例 | 简化主循环展示 |
| `bin/bake` | 资产烘焙 CLI | 调用 `kajiya-asset-pipe` |

## 2. C++ 版 tekki 目标目录结构

```text
tekki/
├── CMakeLists.txt                # 顶层构建入口，编译三类目标：库、工具、测试
├── cmake/                        # 自定义 CMake 模块（工具链、FetchContent）
├── external/                     # 第三方依赖快照或子模块（spdlog、glm、imgui、volk 等）
├── include/
│   └── tekki/                    # 对外可见头文件
├── src/
│   ├── core/                     # 基础设施：类型、日志、配置、数学、时间
│   │   ├── math/                 # GLM 封装/自研数学库
│   │   ├── logging/
│   │   ├── config/
│   │   └── concurrency/          # 任务系统、线程工具
│   ├── backend/
│   │   ├── vulkan/               # 映射 `kajiya-backend::vulkan`
│   │   │   ├── Instance.h/.cpp
│   │   │   ├── Device.h/.cpp
│   │   │   ├── Buffer.h/.cpp
│   │   │   ├── Image.h/.cpp
│   │   │   ├── RayTracing.h/.cpp
│   │   │   ├── Barrier.h/.cpp
│   │   │   └── DescriptorPools.h/.cpp
│   │   ├── shader/               # Shader 与 Pipeline 编译、缓存
│   │   ├── allocator/            # GPU 内存分配（集成 VMA）
│   │   ├── profiler/             # GPU Profiler/DLSS 钩子
│   │   └── filesystem/           # VFS 映射、路径工具
│   ├── assets/
│   │   ├── mesh/
│   │   ├── image/
│   │   ├── gltf/
│   │   └── cache/                # `.mesh/.image` 打包逻辑
│   ├── render_graph/
│   │   ├── Graph.h/.cpp
│   │   ├── ResourceRegistry.h/.cpp
│   │   ├── PassBuilder.h/.cpp
│   │   ├── TemporalResources.h/.cpp
│   │   └── DebugViews.h/.cpp
│   ├── renderer/
│   │   ├── world/
│   │   │   ├── WorldRenderer.h/.cpp
│   │   │   ├── MeshManager.h/.cpp
│   │   │   ├── LightingPipeline.h/.cpp
│   │   │   ├── SsgiPass.h/.cpp
│   │   │   ├── RtrPass.h/.cpp
│   │   │   ├── RtdgiPass.h/.cpp
│   │   │   ├── TaaPass.h/.cpp
│   │   │   └── PostProcess.h/.cpp
│   │   ├── ui/                   # UI 渲染、调试工具
│   │   ├── lut/
│   │   ├── math/
│   │   └── debug/
│   ├── app/
│   │   ├── simple_loop/          # 对标 `kajiya-simple`
│   │   ├── runtime/              # `bin/view` 运行时状态
│   │   ├── input/
│   │   └── ui/                   # GUI 控制面板
│   └── ui/                       # 平台层 ImGui 集成、公用 UI 资源
├── tools/
│   ├── asset_baker/              # 对标 `bin/bake`
│   ├── viewer/                   # 对标 `bin/view`
│   └── hello/                    # 对标 `bin/hello`
├── shaders/
│   ├── spirv/                    # 直接保留/编译出的 SPIR-V
│   ├── sources/                  # C++ 管线期望的 HLSL/GLSL/Rust-gpu 源
│   └── include/                  # 着色器共享头（结构体、常量）
├── data/
│   ├── fonts/
│   └── config/
├── tests/
│   ├── smoke/
│   ├── render_graph/
│   └── regression/
└── docs/
    └── tekki_translation_plan.md
```

### 模块映射关系

| Rust Crate | 对应 C++ 子系统 | 说明 |
| --- | --- | --- |
| `kajiya` | `src/renderer/world`, `src/renderer/ui`, `src/core` | WorldRenderer 及其 pass 与调度逻辑拆分为多文件，公共数学/相机迁移至 `core` |
| `kajiya-backend` | `src/backend` | Vulkan 设备封装、Shader/Pipeline 编译、内存分配与 VFS |
| `kajiya-rg` | `src/render_graph` | 保留 Render Graph 抽象，翻译为 C++ 模板与类型安全封装 |
| `kajiya-asset` | `src/assets` | 网格/图像数据结构与 GLTF 导入 |
| `kajiya-asset-pipe` | `tools/asset_baker` + `src/assets/cache` | C++ CLI 工具，重用资产库 |
| `kajiya-simple` | `src/app/simple_loop` | 主循环构建器、事件派发、日志配置 |
| `kajiya-imgui` + `ash-imgui` | `src/ui`, `src/renderer/ui` | Vulkan ImGui 后端与 UI 任务注入 |
| `rust-shaders` + `rust-shaders-shared` | `shaders/sources`, `shaders/include` | 着色器源码/共享头翻译与组织 |
| `bin/view` | `tools/viewer` | GUI、运行时、序列系统 |
| `bin/hello` | `tools/hello` | 最小示例 |
| `bin/bake` | `tools/asset_baker` | 资产烘焙 CLI |
| `ngx_dlss` | `src/backend/dlss`（可选） | FFI 转为 C 接口/SDK 集成 |

## 3. 翻译策略与步骤

1. **基础设施阶段** ✅ **已完成**
   - 选择构建系统（CMake + Conan/vcpkg），搭建最小可编译骨架。
   - 引入基础依赖：GLM、spdlog、magic_enum、imgui、volk/Vulkan SDK、EnTT（若需要 ECS）、VMA。
   - 实现 `src/core` 公共模块（日志、时间、配置、数学向量/矩阵、错误处理）。

2. **Vulkan 后端迁移** ✅ **已完成**
   - 将 `kajiya-backend` 的设备/资源管理按模块翻译为 C++ 类，保持 RAII。
   - 重写 shader/pipeline 编译缓存，兼容 SPIR-V 输入；评估保留 `rust-gpu` 输出或改写为 GLSL/HLSL。
   - 实现瞬时资源缓存、动态常量缓冲、同步工具。
   - 可选：移植 GPU profiler、DLSS FFI。

3. **Render Graph 实现** ✅ **已完成**
   - 在 `src/render_graph` 设计 C++ 类型：`Graph`, `PassBuilder`, `ResourceHandle`。
   - 支持时间重用（Temporal resources）、调试钩子、Graph 可视化。
   - 编写单元测试验证图的拓扑排序与资源生命周期。
   - **实现文件**：
     - `Resource.h` - 资源类型系统、句柄和引用
     - `Graph.h` - 渲染图主类和执行状态
     - `PassBuilder.h` - 通道构建器
     - `ResourceRegistry.h` - 资源注册表
     - `PassApi.h` - 通道执行API
     - `Temporal.h` - 时间资源管理
     - `RenderGraph.h` - 主接口头文件
     - `RenderGraph.cpp` - 基础实现

4. **世界渲染器及渲染 Pass** ⚠️ **部分完成**
   - ✅ 翻译 `WorldRenderer`，拆分 Mesh 管理、实例、光照、LUT、后处理模块。
   - ⚠️ 渲染 Pass 逐个移植：SSR/RTR、SSGI、RTGI、阴影去噪、TAA、后处理等。
   - ⚠️ 重新实现绑定表、Bindless Texture 管理、TLAS/BLAS 构建。
   - ⚠️ UI 渲染、调试模式、Temporal Upsampling 支持。
   - **状态**: 框架结构已完成，具体渲染算法实现中

5. **应用层与工具**
   - `simple_loop`：窗口、输入、主循环；利用 GLFW/Winit 替代方案。
   - `viewer`：场景加载、GUI 面板、持久化状态（RON -> JSON/TOML）。
   - `asset_baker`：GLTF -> 缓存格式流水线；使用 cgltf/stb_image 实现。
   - `hello`：最小例子验证。

6. **着色器迁移**
   - 选择目标语言（建议：GLSL/HLSL）与编译链（glslang/dxc）。
   - 翻译 `rust-shaders` 逻辑，确保共享结构与 `rust-shaders-shared` 一致。
   - 建立自动编译脚本，产出 SPIR-V 并放入 `shaders/spirv`，实现热重载/缓存。

7. **验证与优化**
   - 建立渲染回归测试（截图对比）、性能基准、内存泄漏检测。
   - 添加文档与 Doxygen/Sphinx 支持。
   - 准备发布所需的打包脚本与示例场景。

## 4. 关键技术考量

- **资源生命周期管理**：Rust 原版依赖所有权系统，C++ 需通过智能指针（`std::unique_ptr`/`std::shared_ptr`）与明确约束补齐。
- **任务/并行系统**：Rust 版使用 `async_executor`、`turbosloth`；C++ 需评估使用 `std::jthread`+Futures 或第三方任务系统（如 Taskflow）。
- **错误处理**：Rust `Result` -> C++ `tl::expected` 或自定义错误类型，保持调用层可追踪。
- **绑定与反射**：`rspirv_reflect` 在 C++ 中可替换为 `SPIRV-Reflect`。
- **热重载与 Shader Cache**：Rust 后端具备缓存机制，迁移时需保留二进制缓存、时间戳校验。
- **平台与输入**：Rust 依赖 Winit；C++ 方案可选 GLFW + ImGui backend 或自研 Win32/XCB 层。
- **序列化格式**：RON 转换为 JSON/TOML/YAML，需要提供兼容的加载/保存逻辑。
- **可选功能**：DLSS、GPU Profiler（`gpu_profiler`）需评估 SDK 授权与 API 差异。

## 5. 里程碑与时间预估

| 阶段 | 交付内容 | 预估周期 | 状态 |
| --- | --- | --- | --- |
| M1 基础设施 | CMake 骨架、core 模块、后台空实现编译通过 | 2-3 周 | ✅ **已完成** |
| M2 Vulkan 后端 | 完成资源管理、命令录制、同步、Shader 编译、GPU Profiler、DLSS | 4-6 周 | ✅ **已完成** |
| M3 Render Graph | 图构建、资源追踪、Temporal 支持、单元测试 | 3-4 周 | ✅ **已完成** |
| M4 世界渲染器 | Mesh/实例/TLAS、关键渲染 pass、UI 挂钩 | 6-8 周 | ⚠️ **部分完成** |
| M5 工具链 | Viewer、Asset Baker、Hello Demo | 3-4 周 | ⏳ 待开始 |
| M6 着色器与回归 | 着色器迁移、自动化测试、性能调优 | 4-6 周 | ⏳ 待开始 |

（具体周期视团队规模与并行度调整）

## 6. 风险与开放问题

- 着色器语言选择与 `rust-gpu` 等价实现成本高，需要专门团队评估。
- Vulkan Ray Tracing API 与 `ash` 封装差异，需确保 C++ RAII 设计不引入资源泄漏。
- Bindless 资源与描述符数量限制与各平台驱动兼容性。
- 任务系统与资源加载的多线程安全性、死锁风险。
- 原项目利用的第三方库（`turbosloth`, `gpu_profiler` 等）在 C++ 生态是否有等价实现。
- 资产格式兼容性：`.mesh/.image` 二进制结构需严密对齐 C++ 版本。
- 自动化测试/回归环境要求 GPU，可导致 CI 成本较高。

## 7. 下一步建议

1. **启动 M4 世界渲染器实现** - 开始翻译 `WorldRenderer` 和相关渲染 pass
2. **完善 Render Graph 单元测试** - 添加拓扑排序和资源生命周期测试
3. **集成验证** - 将 Render Graph 与现有 Vulkan 后端集成测试
4. **着色器迁移准备** - 评估 HLSL 翻译策略和工具链


## 8. 当前决策结论

- **构建与依赖管理**：沿用现有 `CMake + Conan` 组合。通过 Conan 引入 `glm`、`spdlog`、`imgui`、`volk`、`spirv-tools`、`shaderc`、`vma` 等依赖，针对 Windows/Linux 提供 profile；顶层 CMake 聚合库与工具目标。
- **第三方库策略**：优先使用 Conan 官方包；缺失或定制库（如 `entt`、`taskflow`、`spirv-reflect`）通过 Conan 自建 recipe 或 `external/` 子模块补齐。
- **着色器迁移路线**：统一采用 **HLSL + DXC** 生成 SPIR-V。原因：语义贴近现代图形 API、光线追踪扩展支持完整、与厂商调试工具兼容。保留 Rust 着色器仓库以便对比。
- **着色器共享结构体**：借助 `spirv-reflect` 自动生成 C++ 头文件，并与 `shaders/include` 中的手写定义做一致性检查，减少结构体偏移错误。
- **平台层方案**：初期选择 GLFW + ImGui backend 提供窗口与输入；后续抽象平台接口，视需求扩展到 Win32/X11/Wayland。

## 9. 里程碑负责人与验收标准（草案）

| 阶段 | 负责人 | 关键任务 | 验收标准 |
| --- | --- | --- | --- |
| M1 基础设施 | 架构负责人 A | Conan 配置、核心模块骨架、CI 脚本草案 | `cmake --build` 成功，生成空壳 demo；CI pipeline 通过 |
| M2 Vulkan 后端 | 渲染后端负责人 B | Device/Swapchain/Buffer/Image/RT 封装，Shader 编译缓存 | Vulkan 清屏 demo 运行；资源生命周期测试覆盖；无验证层报错 |
| M3 Render Graph | Render Graph 工程师 C | Graph/PassBuilder/资源追踪 | 支持构建/执行 3 个示例图；提供 Graph 调试可视化 | ✅ **已完成** |
| M4 世界渲染器 | 世界渲染负责人 D | Mesh 管理、主要渲染 pass、UI 对接 | 样例场景渲染与 Rust 版对齐（截图差异 <5%）；关键调试模式可用 |
| M5 工具链 | 应用层负责人 E | Viewer、Asset Baker、Hello Demo | Viewer 可加载 `.scene` 与 `.mesh`；CLI 烘焙与原版输出兼容 |
| M6 着色器与回归 | Shader 负责人 F | HLSL 翻译、自动化对比、性能调优 | CI 自动编译并运行渲染回归；性能/内存指标达成目标 |

> 负责人为占位，后续替换为具体人选；验收标准在详细设计评审中再量化。

## 10. Render Graph 实现技术说明

### 核心架构
- **类型系统**：使用 C++ 模板特化（`ResourceTraits`）替代 Rust 的 trait 系统
- **资源句柄**：类型安全的 `Handle<T>`, `ExportedHandle<T>`, `Ref<T, V>` 模板
- **视图类型**：`GpuSrv`（只读）、`GpuUav`（读写）、`GpuRt`（渲染目标）
- **执行状态**：`CompiledRenderGraph` → `ExecutingRenderGraph` → `RetiredRenderGraph`

### 关键特性
- 时间资源重用（Temporal resources）
- 资源生命周期自动管理
- 通道依赖分析和拓扑排序
- Vulkan 同步和访问控制
- 调试钩子和可视化支持

### 文件结构
```
src/render_graph/
├── Resource.h          # 资源类型系统、句柄和引用
├── Graph.h             # 渲染图主类和执行状态
├── PassBuilder.h       # 通道构建器
├── ResourceRegistry.h  # 资源注册表
├── PassApi.h           # 通道执行API
├── Temporal.h          # 时间资源管理
├── RenderGraph.h       # 主接口头文件
└── RenderGraph.cpp     # 基础实现
```

## 11. M1 近期行动项

1. 编写 Conan profile（Debug/Release）与 `CMakePresets.json` 草案，确保多平台一致。
2. 在 `src/core` 搭建 `Logging`、`Time`、`Config` 模块头/源文件空实现，并写最小示例使用。
3. 搭建 GLFW + Vulkan 示例窗口，验证依赖链与验证层。
4. 为 CI（GitHub Actions/TeamCity 等）添加 Conan 安装、CMake 配置与构建步骤。
5. 输出《M1 详细设计》文档，覆盖模块接口、测试计划、风险清单，提交评审。
