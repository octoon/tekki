# tekki 翻译文档

本文档用于跟踪 kajiya 从 Rust 翻译到 C++ 的整体进度。

**当前状态**：已完成基础设施、Vulkan后端、Render Graph模块，准备开始世界渲染器实现。

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

### 阶段四：资源管线 ⏳ 待开始
- [ ] glTF 加载器
- [ ] 图像加载器
- [ ] 纹理压缩
- [ ] 材质系统

### 阶段五：渲染器 ⏳ 待开始
- [ ] 光栅化通道
- [ ] 光线追踪通道
- [ ] 全局光照实现
- [ ] 去噪
- [ ] 后处理

### 阶段六：查看器 ⏳ 待开始
- [ ] 窗口创建
- [ ] 输入处理
- [ ] ImGui 集成
- [ ] 场景加载

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

## 构建

构建流程详见 [README.md](../README.md)。

## 贡献

翻译代码时请遵循以下原则：
1. 保持尽可能接近的模块结构
2. 优先使用现代 C++20 特性
3. 记录无法直接对应的 Rust 模式
4. 在重要位置添加原始 Rust 代码的引用
5. 按进度更新本文档
