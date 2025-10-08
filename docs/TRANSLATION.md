# tekki 翻译文档

本文档用于跟踪 kajiya 从 Rust 翻译到 C++ 的整体进度。

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

### 阶段一：基础设施（当前）
- [x] 项目初始化
- [x] 构建系统（CMake + Conan）
- [x] 基础文档
- [ ] 核心工具
- [ ] 日志系统
- [ ] 数学库集成

### 阶段二：后端
- [ ] Vulkan 设备初始化
- [ ] 内存分配器
- [ ] 命令缓冲管理
- [ ] 管线管理
- [ ] 描述符管理

### 阶段三：渲染图
- [ ] 图构建
- [ ] 资源追踪
- [ ] 屏障插入
- [ ] 执行逻辑

### 阶段四：资源管线
- [ ] glTF 加载器
- [ ] 图像加载器
- [ ] 纹理压缩
- [ ] 材质系统

### 阶段五：渲染器
- [ ] 光栅化通道
- [ ] 光线追踪通道
- [ ] 全局光照实现
- [ ] 去噪
- [ ] 后处理

### 阶段六：查看器
- [ ] 窗口创建
- [ ] 输入处理
- [ ] ImGui 集成
- [ ] 场景加载

## 构建

构建流程详见 [README.md](../README.md)。

## 贡献

翻译代码时请遵循以下原则：
1. 保持尽可能接近的模块结构
2. 优先使用现代 C++20 特性
3. 记录无法直接对应的 Rust 模式
4. 在重要位置添加原始 Rust 代码的引用
5. 按进度更新本文档
