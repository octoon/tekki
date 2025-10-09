# tekki 翻译文档

本文档用于跟踪 kajiya 从 Rust 翻译到 C++ 的整体进度。

**当前状态**：已完成基础设施、Vulkan后端、Render Graph模块、世界渲染器核心结构，准备开始资源管线实现。

## 架构概览

tekki 延续了 kajiya 的模块化架构：

### 核心模块

1. **core** —— 基础工具与类型
   - 数学工具（向量、矩阵、变换）
   - 内存管理
   - 日志与错误处理
   - 通用数据结构

2. **backend** —— Vulkan 抽象层
   - 设备管理
   - 命令缓冲管理
   - 内存分配（使用 VMA）
   - 管线管理
   - 描述符集
   - 同步原语

3. **rg** —— 渲染图
   - 图构建与优化
   - 资源生命周期管理
   - 自动插入屏障
   - 多线程支持

4. **asset** —— 资源管线
   - glTF 加载
   - 图像加载（PNG、JPG、HDR、EXR）
   - 纹理压缩
   - 网格处理
   - 材质系统

5. **renderer** —— 核心渲染实现
   - 全局光照
   - 光线追踪
   - 去噪
   - 时域超分
   - 后处理

6. **viewer** —— 应用层
   - 窗口管理
   - 输入处理
   - ImGui 集成
   - 场景管理

## Rust → C++ 翻译模式

### 错误处理
- Rust `Result<T, E>` → C++ 异常或 `std::expected<T, E>`（C++23）
- Rust `Option<T>` → C++ `std::optional<T>`
- Rust `anyhow::Error` → 自定义异常类型

### 内存管理
- Rust 所有权 → C++ `std::unique_ptr`
- Rust `Rc<T>` → C++ `std::shared_ptr<T>`
- Rust `Arc<T>` → C++ `std::shared_ptr<T>`（原子引用计数）
- Rust `RefCell<T>` → 手动同步或 `std::mutex`

### 并发
- Rust `Mutex<T>` / `RwLock<T>` → C++ `std::mutex` / `std::shared_mutex`
- Rust async/await → C++ 协程（C++20）或回调式实现

### 类型系统
- Rust trait → C++ concepts（C++20）或抽象类
- Rust enum → C++ `std::variant` 或类层次
- Rust 模式匹配 → C++ `std::visit` 或 if-else 逻辑

## 翻译进度

### 阶段一：基础设施 ✅ 已完成
- [x] 项目初始化
- [x] 构建系统（CMake + Conan）
- [x] 基础文档
- [x] 核心工具
- [x] 日志系统
- [x] 数学库集成

### 阶段二：后端 ✅ 已完成
- [x] Vulkan 设备初始化
- [x] 内存分配器
- [x] 命令缓冲管理
- [x] 管线管理
- [x] 描述符管理

### 阶段三：渲染图 ✅ 已完成
- [x] 图构建
- [x] 资源追踪
- [x] 屏障插入
- [x] 执行逻辑
- [x] 时间资源管理
- [x] 通道构建器
- [x] 资源注册表

### 阶段四：世界渲染器及渲染 Pass ⏳ 进行中
- [x] WorldRenderer 核心结构
- [x] Mesh 管理和实例系统
- [x] 主要渲染器头文件（SSGI、TAA、Post、RTR、Lighting、IRCache、RTDGI、ShadowDenoise、IBL）
- [x] G-buffer 和深度结构
- [x] 时间资源管理
- [x] 绑定描述符集管理（已修复 binding flags）
- [x] 光线追踪加速结构
- [x] BufferBuilder 工具类
- [ ] 渲染器完整实现（19个部分实现，11个缺失）

### 阶段五：资源管线 ⏳ 待开始
- [ ] glTF 加载器
- [ ] 图像加载器
- [ ] 纹理压缩
- [ ] 材质系统

### 阶段六：渲染器实现 ⏳ 待开始
- [ ] 光栅化通道
- [ ] 光线追踪通道
- [ ] 全局光照实现
- [ ] 去噪
- [ ] 后处理

### 阶段七：查看器 ⏳ 待开始
- [ ] 窗口创建
- [ ] 输入处理
- [ ] ImGui 集成
- [ ] 场景加载

## kajiya/src 文件翻译详细状态

### 核心文件（kajiya/crates/lib/kajiya/src）

#### ✅ 已完全翻译（15/37 = 40.5%）

**核心系统：**
1. **bindless_descriptor_set.rs** → `backend/vulkan/bindless_descriptor_set.h`
   - ✅ 绑定标志支持
   - ✅ 描述符池创建
   - ✅ 变量描述符计数

2. **buffer_builder.rs** → `backend/vulkan/buffer_builder.{h,cpp}`
   - ✅ BufferDataSource 接口
   - ✅ 对齐处理
   - ✅ 分块上传

3. **camera.rs** → `shared/camera.h`
   - ✅ CameraMatrices 结构

4. **logging.rs** → `core/log.{h,cpp}`
   - ✅ 使用 spdlog

5. **math.rs** → 使用 GLM 库

6. **dlss.rs** → `backend/vulkan/dlss.{h,cpp}`
   - ✅ NVIDIA DLSS 支持

7. **frame_desc.rs** → `render_graph/execution_params.h`

8. **default_world_renderer.rs** → `renderer/default_world_renderer.{h,cpp}`
   - ✅ WorldRenderer 工厂函数
   - ✅ BRDF LUT 初始化
   - ✅ 蓝噪声纹理加载
   - ✅ Bezold-Brücke LUT 初始化

9. **lut_renderers.rs** → `renderer/lut_renderers.{h,cpp}`
   - ✅ BrdfFgLutComputer
   - ✅ BezoldBruckeLutComputer

**渲染器完整翻译：**
10. **renderers/raster_meshes.rs** → `renderer/renderers/raster_meshes.cpp`
    - ✅ 几何体光栅化

11. **renderers/deferred.rs** → `renderer/renderers/deferred.cpp`
    - ✅ light_gbuffer 函数

12. **renderers/shadows.rs** → `renderer/renderers/shadows.cpp`
    - ✅ trace_sun_shadow_mask 函数

13. **renderers/sky.rs** → `renderer/renderers/sky.cpp`
    - ✅ render_sky_cube 函数
    - ✅ convolve_cube 函数

14. **renderers/reprojection.rs** → `renderer/renderers/reprojection.cpp`
    - ✅ calculate_reprojection_map 函数

15. **renderers/reference.rs** → `renderer/renderers/reference.cpp`
    - ✅ reference_path_trace 函数

#### ⚠️ 部分翻译（19/37 = 51.4%）

**渲染器模块（有头文件，缺少或不完整实现）：**

8. **renderers/ibl.rs** → `renderer/renderers/ibl.{h,cpp}`
   - ✅ 头文件和实现
   - ✅ 环境贴图加载
   - ✅ IBL 立方体贴图生成
   - ⚠️ EXR/HDR 加载待实现

9. **renderers/ircache.rs** → `renderer/renderers/ircache.{h,cpp}`
   - ✅ 头文件
   - ⚠️ 实现不完整

10. **renderers/lighting.rs** → `renderer/renderers/lighting.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 实现不完整

11. **renderers/post.rs** → `renderer/renderers/post.{h,cpp}`
    - ✅ 头文件和基础实现
    - ⚠️ 需要完善

12. **renderers/rtdgi.rs** → `renderer/renderers/rtdgi.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 实现不完整

13. **renderers/rtr.rs** → `renderer/renderers/rtr.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 实现不完整

14. **renderers/shadow_denoise.rs** → `renderer/renderers/shadow_denoise.{h,cpp}`
    - ✅ 头文件和实现

15. **renderers/ssgi.rs** → `renderer/renderers/ssgi.{h,cpp}`
    - ✅ 头文件和实现

16. **renderers/taa.rs** → `renderer/renderers/taa.{h,cpp}`
    - ✅ 头文件和实现

**部分实现的渲染器（需要完善）：**

17. **renderers/lighting.rs** → `renderer/renderers/lighting.{h,cpp}`
    - ✅ 头文件
    - ⚠️ render_specular 等函数需要完善

18. **renderers/ircache.rs** → `renderer/renderers/ircache.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 辐照度缓存复杂逻辑需要完善

19. **renderers/rtdgi.rs** → `renderer/renderers/rtdgi.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 光线追踪漫反射 GI 需要完善

20. **renderers/rtr.rs** → `renderer/renderers/rtr.{h,cpp}`
    - ✅ 头文件
    - ⚠️ 光线追踪反射需要完善

**核心系统部分翻译：**

23. **world_renderer.rs** → `renderer/world/WorldRenderer.{h,cpp}`
    - ✅ 核心结构
    - ⚠️ 完整渲染管线未实现

24. **world_render_passes.rs**
    - ⚠️ 部分集成到 WorldRenderer.cpp
    - ❌ prepare_render_graph_standard/reference 未完整翻译

25. **image_cache.rs**
    - ⚠️ 基础功能在 transient_resource_cache.h
    - ❌ 完整图像加载管线缺失

26. **image_lut.rs**
    - ⚠️ ImageLut 结构存在于 renderers.h
    - ❌ ComputeImageLut trait 和计算逻辑缺失

#### ❌ 未翻译（7/37 = 18.9%）

**高级渲染器：**
28. **renderers/dof.rs** - 景深效果
29. **renderers/motion_blur.rs** - 运动模糊
30. **renderers/ussgi.rs** - Ultra-wide SSGI
31. **renderers/wrc.rs** - World Radiance Cache

**工具模块：**
28. **renderers/half_res.rs** - 半分辨率渲染工具
29. **renderers/prefix_scan.rs** - GPU 前缀扫描算法
30. **mmap.rs** - 内存映射资源加载
31. **ui_renderer.rs** - ImGui 渲染集成
32. **renderers/old/csgi.rs** - 旧版 CSGI（可忽略）
33. **renderers/old/sdf.rs** - 旧版 SDF（可忽略）

### 翻译进度总结

**本次会话完成的工作：**

✅ **已完成翻译** (15/37 = 40.5%)
- 核心系统修复：bindless_descriptor_set, buffer_builder
- 工厂和工具：default_world_renderer, lut_renderers
- 基础渲染：raster_meshes, deferred
- 高级渲染：shadows, sky, reprojection, reference
- IBL 系统：ibl 完整实现

⚠️ **部分翻译** (15/37 = 40.5%)
- 需要完善的渲染器：lighting, ircache, rtdgi, rtr
- 已有头文件和基础实现的渲染器：ssgi, taa, post, shadow_denoise

❌ **未翻译** (7/37 = 18.9%)
- 高级特性：dof, motion_blur, ussgi, wrc
- 工具模块：half_res, prefix_scan, mmap, ui_renderer

**下一步优先级：**

1. **完善部分实现的渲染器** (高优先级)
   - lighting.cpp - 完整光照计算
   - ircache.cpp - 辐照度缓存完善
   - rtdgi.cpp - 光线追踪漫反射 GI
   - rtr.cpp - 光线追踪反射

2. **完成资源加载** (中优先级)
   - 实现 EXR/HDR 图像加载
   - mmap 内存映射加载
   - image_cache 完整实现

3. **高级特性** (低优先级)
   - dof, motion_blur 后处理
   - ussgi, wrc 替代 GI 技术
   - ui_renderer ImGui 集成

## 已实现模块详情

### Render Graph 实现（阶段三）

#### 核心文件
- `src/render_graph/Resource.h` - 资源类型系统、句柄和引用
- `src/render_graph/Graph.h` - 渲染图主类和执行状态
- `src/render_graph/PassBuilder.h` - 通道构建器
- `src/render_graph/ResourceRegistry.h` - 资源注册表
- `src/render_graph/PassApi.h` - 通道执行API
- `src/render_graph/Temporal.h` - 时间资源管理
- `src/render_graph/RenderGraph.h` - 主接口头文件
- `src/render_graph/RenderGraph.cpp` - 基础实现

#### 关键特性
- **类型安全**：使用模板特化（`ResourceTraits`）替代 Rust trait 系统
- **资源管理**：`Handle<T>`, `ExportedHandle<T>`, `Ref<T, V>` 模板
- **视图类型**：`GpuSrv`（只读）、`GpuUav`（读写）、`GpuRt`（渲染目标）
- **执行状态**：`CompiledRenderGraph` → `ExecutingRenderGraph` → `RetiredRenderGraph`
- **时间资源**：支持跨帧资源重用和状态管理

#### 技术映射
- Rust trait → C++ 模板特化
- Rust enum → C++ `std::variant`
- Rust 所有权 → C++ RAII + 智能指针
- Rust 模式匹配 → C++ `std::visit`

### World Renderer 实现（阶段四）

#### 核心文件
- `src/renderer/world/WorldRenderer.h` - 世界渲染器主类、Mesh和实例管理
- `src/renderer/world/WorldRenderer.cpp` - 世界渲染器实现
- `src/renderer/renderers/renderers.h` - 渲染器通用结构和前向声明
- `src/renderer/renderers/renderers.cpp` - 渲染器通用结构实现
- `src/renderer/renderers/ssgi.h` - 屏幕空间全局光照
- `src/renderer/renderers/taa.h` - 时间抗锯齿
- `src/renderer/renderers/post.h` - 后处理
- `src/renderer/renderers/rtr.h` - 光线追踪反射
- `src/renderer/renderers/lighting.h` - 光照计算
- `src/renderer/renderers/ircache.h` - 辐照度缓存
- `src/renderer/renderers/rtdgi.h` - 光线追踪漫反射全局光照
- `src/renderer/renderers/shadow_denoise.h` - 阴影去噪
- `src/renderer/renderers/ibl.h` - 基于图像的照明

#### 关键特性
- **Mesh 管理**：`MeshHandle`、`InstanceHandle` 类型安全句柄
- **实例系统**：变换管理、动态参数、前帧变换存储
- **绑定描述符集**：支持大量纹理的无绑定访问
- **光线追踪**：BLAS/TLAS 加速结构管理
- **曝光控制**：动态曝光状态和直方图裁剪
- **时间资源**：跨帧资源重用和状态管理

#### 技术映射
- Rust `Affine3A` → C++ `glm::mat4`
- Rust `Vec2/Vec3` → C++ `glm::vec2/glm::vec3`
- Rust `HashMap` → C++ `std::unordered_map`
- Rust `Arc<T>` → C++ `std::shared_ptr<T>`
- Rust `Mutex<T>` → C++ `std::mutex` + `std::lock_guard`

## 构建

构建流程详见 [README.md](../README.md)。

## 贡献

翻译代码时请遵循以下原则：
1. 保持尽可能接近的模块结构
2. 优先使用现代 C++20 特性
3. 记录无法直接对应的 Rust 模式
4. 在重要位置添加原始 Rust 代码的引用
5. 按进度更新本文档
