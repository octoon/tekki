# tekki 开发指南

## 入门

### 前置条件

- 支持 C++20 的编译器（GCC 10+、Clang 11+、MSVC 2019+）
- CMake 3.20 及以上
- Conan 2.0 及以上
- Vulkan SDK
- Python 3.6+（用于 Conan）

### 配置开发环境

1. **安装 Conan**：
```bash
pip install conan
```

2. **检测 Conan 配置文件**（首次执行）：
```bash
conan profile detect --force
```

3. **克隆仓库**：
```bash
git clone <repository-url>
cd tekki
```

4. **安装依赖**：
```bash
mkdir build && cd build
conan install .. --output-folder=. --build=missing -s build_type=Debug
```

5. **配置并构建**：
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## 项目结构

```
tekki/
├── src/
│   ├── core/           # 核心工具（数学、内存等）
│   ├── backend/        # Vulkan 后端抽象
│   ├── rg/             # 渲染图系统
│   ├── asset/          # 资源加载与管理
│   ├── renderer/       # 核心渲染实现
│   └── viewer/         # 查看器应用
├── shaders/            # 着色器源码（HLSL）
├── assets/             # 示例资源与测试场景
├── docs/               # 文档
├── kajiya/             # 原始 Rust 源码（参考）
├── CMakeLists.txt      # 根 CMake 文件
├── conanfile.py        # Conan 依赖声明
└── README.md           # 项目说明
```

## 代码规范

### C++ 风格指南

- **命名约定**：
  - 类：`PascalCase` （例：`RenderGraph`）
  - 函数：`snake_case` （例：`create_device`）
  - 变量：`snake_case` （例：`vertex_count`）
  - 常量：`SCREAMING_SNAKE_CASE` （例：`MAX_FRAMES_IN_FLIGHT`）
  - 命名空间：`snake_case` （例：`tekki::backend`）

- **文件组织**：
  - 头文件使用 `.hpp`
  - 实现文件使用 `.cpp`
  - 通常一个类对应一对头/源文件
  - 文件名与类名保持一致：`render_graph.hpp` / `render_graph.cpp`

- **头文件保护**：
  - 使用 `#pragma once`

- **格式化**：
  - 4 个空格缩进（禁止使用制表符）
  - 单行建议不超过 100 字符
  - 函数与控制结构的大括号放在同一行

### 错误处理

- 使用异常处理错误场景
- 针对不同的错误类别定义自定义异常类型
- 在函数注释中说明可能抛出的异常

示例：
```cpp
class VulkanError : public std::runtime_error {
public:
    explicit VulkanError(const std::string& msg) : std::runtime_error(msg) {}
};

void check_vk_result(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw VulkanError(std::string("Vulkan error in ") + operation);
    }
}
```

### 内存管理

- 首选 RAII 与智能指针
- 独占所有权使用 `std::unique_ptr`
- 仅在确有需要时使用 `std::shared_ptr`
- 避免直接调用 `new`/`delete`

### 现代 C++ 特性

优先使用 C++20 功能：
- `std::span` 用于视图
- `std::optional` 表示可选值
- `std::variant` 表示和类型
- Concepts 用于模板约束
- Ranges 提升迭代表达力

## 翻译指南

从 Rust 翻译到 C++ 时：

1. **保持结构**：尽量复刻 kajiya 的模块结构
2. **记录来源**：在关键位置注明对应的 Rust 源码
3. **渐进测试**：每完成一个子模块就验证功能
4. **更新文档**：同步维护 TRANSLATION.md 的进度

### 常见翻译模式

**Rust `Result<T, E>`** → C++ 异常或返回码：
```rust
// Rust
fn create_device() -> Result<Device, DeviceError> {
    // ...
}
```

```cpp
// C++（使用异常）
Device create_device() {
    // 失败时抛出 DeviceError
}
```

**Rust `Option<T>`** → C++ `std::optional<T>`：
```rust
// Rust
fn find_queue_family() -> Option<u32> {
    // ...
}
```

```cpp
// C++
std::optional<uint32_t> find_queue_family() {
    // ...
}
```

**Rust 所有权** → C++ 智能指针：
```rust
// Rust
struct Renderer {
    device: Device,
}
```

```cpp
// C++
class Renderer {
    std::unique_ptr<Device> device;
};
```

## 构建与测试

### Debug 构建
```bash
cmake --build build --config Debug
```

### Release 构建
```bash
cmake --build build --config Release
```

### 运行查看器
```bash
./build/bin/tekki-view
```

### 调试层（Debug）
调试构建默认启用 Vulkan 验证层。如需禁用：
```bash
cmake .. -DTEKKI_WITH_VALIDATION=OFF
```

## 文档撰写

- 对公共 API 编写 Doxygen 风格注释
- 对复杂算法补充行内说明
- 新增重大功能时同步更新 docs/

示例：
```cpp
/**
 * @brief 创建带所需功能的 Vulkan 设备
 *
 * @param physical_device 目标物理设备
 * @param extensions 所需的设备扩展列表
 * @return std::unique_ptr<Device> 成功创建的设备
 * @throws VulkanError 当设备创建失败时抛出
 */
std::unique_ptr<Device> create_device(
    VkPhysicalDevice physical_device,
    const std::vector<const char*>& extensions
);
```

## 调试

### Vulkan 验证层

在 Debug 构建中默认启用，关注输出中的验证信息。

### RenderDoc

捕获帧：
```bash
renderdoccmd capture ./build/bin/tekki-view
```

### GDB/LLDB

```bash
gdb --args ./build/bin/tekki-view [args]
```

## 贡献

翻译代码时请参阅 [TRANSLATION.md](TRANSLATION.md)，了解当前进度与指导原则。
