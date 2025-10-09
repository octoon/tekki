# Tekki 编译修复计划

## 概述

基于 kajiya Rust 源码的分析，本文档列出了使 tekki 项目成功编译所需的修复步骤。

## 当前状态

CMakeLists.txt 已更新包含所有源文件，但编译失败，主要问题包括：

1. **文件编码问题**：部分文件含有 markdown 代码块标记
2. **缺失核心头文件**：`tekki/core/Result.h` 等
3. **缺失第三方依赖头文件**：`gpu_allocator.h`、`ash/ash.hpp`、`gpu_profiler`等
4. **缺少标准库 include**

## Kajiya 依赖映射 (Rust -> C++)

基于 `kajiya/crates/lib/kajiya-backend/Cargo.toml`:

| Rust Crate | C++ 替代方案 | 状态 | 说明 |
|-----------|------------|------|------|
| anyhow | 自定义 Result<T> | ❌ 缺失 | 需要创建 `tekki/core/Result.h` |
| ash | Vulkan C API | ✅ 已有 | 直接使用 `vulkan/vulkan.h` |
| gpu-allocator | vulkan-memory-allocator (VMA) | ⚠️ 部分 | 需要创建适配器 |
| gpu-profiler | 自定义实现 | ❌ 缺失 | 需要创建或删除相关代码 |
| rspirv-reflect | spirv-reflect | ✅ 已有 | 需要正确链接 |
| vk-sync | 自定义实现 | ⚠️ 部分 | 部分代码已实现 |
| backtrace | `<stacktrace>` (C++23) | ⚠️ 可选 | 可简化或删除 |
| thiserror | 自定义错误类型 | ❌ 缺失 | 包含在 Result.h 中 |

## 修复计划

### 阶段 1: 清理文件编码问题 (优先级: 🔴 最高)

**问题**: 部分文件含有 markdown 代码块标记 `` ```cpp ``

**受影响文件**:
- `src/backend/vulkan/ray_tracing.cpp` (第1行)
- `src/backend/vulkan/shader.cpp` (第1行)
- `include/tekki/backend/pipeline_cache.h` (第1行, 第138-172行)
- `include/tekki/backend/vulkan/barrier.h` (第1行)

**修复步骤**:
1. 移除所有文件开头的 `` ```cpp `` 标记
2. 移除文件末尾的 `` ``` `` 标记
3. 检查其他可能受影响的文件

### 阶段 2: 创建核心基础设施 (优先级: 🔴 最高)

#### 2.1 创建 Result/Error 处理系统

**目标**: 替代 Rust 的 `anyhow::Result` 和 `thiserror::Error`

**需要创建的文件**:

1. **`include/tekki/core/Result.h`**
   ```cpp
   // 基于 std::expected (C++23) 或自定义实现
   template<typename T, typename E = Error>
   class Result;

   class Error {
       std::string message;
       std::source_location location; // C++20
   };
   ```

2. **`include/tekki/core/Result.hpp`** (别名，某些文件使用 .hpp)
   ```cpp
   #include "tekki/core/Result.h"
   ```

**依赖关系**:
- 被以下文件使用:
  - `backend/lib.h`
  - `backend/dynamic_constants.h`
  - `backend/file.h`
  - `backend/pipeline_cache.h`
  - `backend/rust_shader_compiler.h`
  - `backend/shader_compiler.h`
  - `backend/vulkan/mod.h`
  - `backend/vulkan/ray_tracing.h`
  - `backend/vulkan/shader.h`
  - `backend/vulkan/surface.h`
  - `backend/vulkan/swapchain.h`

**实现选项**:
- **选项 A**: 使用 C++23 `<expected>` (如果编译器支持)
- **选项 B**: 使用第三方库 (如 `tl::expected`)
- **选项 C**: 自定义简化实现 (推荐用于C++20)

#### 2.2 创建 GPU Allocator 适配层

**目标**: 将 `gpu-allocator` (Rust) 适配到 VMA (C++)

**需要创建的文件**:

1. **`include/tekki/backend/vulkan/gpu_allocator.h`**
   ```cpp
   // VMA wrapper 提供类似 gpu-allocator 的接口
   #include <vk_mem_alloc.h>

   namespace tekki::backend::vulkan {
       class Allocator {
           VmaAllocator allocator_;
       public:
           // Allocation, Deallocate 等方法
       };
   }
   ```

**依赖关系**:
- 被以下文件使用:
  - `backend/vulkan/buffer.h`
  - `backend/vulkan/device.h`
  - `backend/vulkan/image.h`
  - `backend/transient_resource_cache.cpp`

**实现**:
- 封装 VMA (vulkan-memory-allocator)
- 提供与 Rust `gpu-allocator` 类似的 API

### 阶段 3: 处理可选依赖 (优先级: 🟡 中等)

#### 3.1 GPU Profiler

**选项 A - 创建占位实现**:
```cpp
// include/tekki/backend/vulkan/gpu_profiler_stub.h
namespace gpu_profiler {
    struct VulkanProfilerFrame {
        // 空实现或基础计时
    };
}
```

**选项 B - 移除相关代码**:
- 从 `backend/vulkan/profiler.h` 移除依赖
- 实现简单的计时器替代

**受影响文件**:
- `include/tekki/backend/vulkan/profiler.h`

#### 3.2 Ash (Vulkan Wrapper)

**当前状态**: 代码尝试包含 `ash/ash.hpp`

**修复方案**:
- Rust 的 `ash` 是 Vulkan API wrapper
- C++ 直接使用 `vulkan/vulkan.h`
- 移除 `ash` 相关 include，使用原生 Vulkan C API

**受影响文件**:
- `include/tekki/backend/vulkan/instance.h`

**修复步骤**:
```cpp
// 替换:
#include <ash/ash.hpp>

// 为:
#include <vulkan/vulkan.h>
```

#### 3.3 vk-sync

**当前状态**: `barrier.h` 中使用了 `vk_sync` namespace

**修复方案**:
- 代码已部分实现 `vk_sync` 的功能
- 检查是否需要额外的 header
- 或创建完整的 `vk_sync.h` 实现

**受影响文件**:
- `include/tekki/backend/vulkan/barrier.h`

### 阶段 4: 修复标准库 Include (优先级: 🟢 低)

**问题**: 某些文件缺少必要的标准库头文件

**需要添加的 includes**:

**`include/tekki/backend/bytes.h`**:
```cpp
#include <stdexcept>  // for std::runtime_error
#include <string>     // for std::string
```

**`include/tekki/backend/chunky_list.h`**:
```cpp
#include <cstddef>    // for std::size_t
#include <vector>     // for std::vector
```

### 阶段 5: 修复模块特定问题 (优先级: 🟡 中等)

#### 5.1 chunky_list.cpp 模板问题

**错误**:
```
C3856: 'TempList': symbol is not a class template
C2989: 'TempList': class template has already been declared as a non-class template
```

**原因**: `TempList` 的声明/定义不一致

**修复**:
- 检查 `include/tekki/backend/chunky_list.h` 中的模板声明
- 确保 `src/backend/chunky_list.cpp` 中的实现匹配

#### 5.2 bytes.cpp 重复定义

**错误**:
```
C2995: function template has already been defined
```

**原因**: 模板函数在头文件已定义，不应在 .cpp 中再次定义

**修复**:
- 移除 `src/backend/bytes.cpp` 中的模板函数定义
- 或将其改为显式实例化

### 阶段 6: 测试编译 (优先级: 🔵 验证)

**步骤**:
1. 完成前面阶段的修复
2. 逐模块编译测试:
   - `tekki-backend`
   - `tekki-rg`
   - `tekki-asset`
   - `tekki-renderer`
   - `tekki-view`
3. 解决剩余的链接错误

## 文件创建清单

### 必须创建 (Phase 2):
- [ ] `include/tekki/core/Result.h`
- [ ] `include/tekki/core/Result.hpp` (软链接或别名)
- [ ] `include/tekki/backend/vulkan/gpu_allocator.h`
- [ ] `src/backend/vulkan/gpu_allocator.cpp`

### 可选创建 (Phase 3):
- [ ] `include/tekki/backend/vulkan/gpu_profiler_stub.h`
- [ ] `include/tekki/backend/vulkan/vk_sync.h` (如果需要)

## 修复优先级建议

### 第一轮 (最关键):
1. ✅ 清理文件编码 (Phase 1)
2. ✅ 创建 `Result.h` (Phase 2.1)
3. ✅ 创建 `gpu_allocator.h` (Phase 2.2)

### 第二轮:
4. 修复 `ash` 依赖 (Phase 3.2)
5. 处理 GPU Profiler (Phase 3.1)
6. 添加缺失的标准库 includes (Phase 4)

### 第三轮:
7. 修复模板问题 (Phase 5)
8. 测试编译 (Phase 6)

## 参考实现

### Result.h 简化实现示例

```cpp
#pragma once
#include <variant>
#include <string>
#include <exception>

namespace tekki::core {

class Error {
public:
    std::string message;

    Error(std::string msg) : message(std::move(msg)) {}

    const char* what() const { return message.c_str(); }
};

template<typename T>
class Result {
    std::variant<T, Error> data_;

public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return std::holds_alternative<Error>(data_); }

    T& value() & { return std::get<T>(data_); }
    T&& value() && { return std::get<T>(std::move(data_)); }

    const T& value() const & { return std::get<T>(data_); }

    Error& error() & { return std::get<Error>(data_); }
    const Error& error() const & { return std::get<Error>(data_); }

    T value_or(T default_value) const {
        if (is_ok()) return value();
        return default_value;
    }

    // Unwrap (throws on error)
    T unwrap() {
        if (is_err()) {
            throw std::runtime_error(error().message);
        }
        return std::get<T>(std::move(data_));
    }
};

// 允许 void 类型
template<>
class Result<void> {
    std::variant<std::monostate, Error> data_;

public:
    Result() : data_(std::monostate{}) {}
    Result(Error error) : data_(std::move(error)) {}

    bool is_ok() const { return std::holds_alternative<std::monostate>(data_); }
    bool is_err() const { return std::holds_alternative<Error>(data_); }

    Error& error() & { return std::get<Error>(data_); }
    const Error& error() const & { return std::get<Error>(data_); }

    void unwrap() {
        if (is_err()) {
            throw std::runtime_error(error().message);
        }
    }
};

} // namespace tekki::core
```

## 时间估算

- Phase 1 (文件清理): 0.5 小时
- Phase 2 (核心基础设施): 2-4 小时
- Phase 3 (可选依赖): 2-3 小时
- Phase 4 (标准库 includes): 0.5 小时
- Phase 5 (模板修复): 1-2 小时
- Phase 6 (测试): 1-2 小时

**总计**: 7-12 小时

## 成功标准

1. ✅ 所有源文件编码正确
2. ✅ 核心头文件 (`Result.h`) 可用
3. ✅ 所有模块独立编译成功
4. ✅ `tekki-backend` 库链接成功
5. ✅ `tekki-view` 可执行文件构建成功
6. ✅ 运行时无 crash (基础测试)

## 后续工作

构建成功后的后续任务:
1. 实现缺失的渲染器功能
2. 移植 shader 系统
3. 添加单元测试
4. 性能优化
5. 文档完善
