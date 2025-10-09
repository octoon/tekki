# GPU Profiler 和 GPU Allocator 模块

## 概述

这两个模块是从原始 Rust 项目完整翻译到 C++ 的子系统:

- **gpu_profiler**: GPU 性能分析工具
- **gpu_allocator**: GPU 内存分配器

## 目录结构

```
tekki/
├── include/tekki/
│   ├── gpu_profiler/
│   │   ├── gpu_profiler.h           # 核心性能分析器接口
│   │   └── backend/
│   │       └── vulkan.h             # Vulkan 后端实现
│   └── gpu_allocator/
│       ├── result.h                 # 错误处理
│       ├── allocator.h              # 分配器基础接口
│       ├── allocator/
│       │   ├── dedicated_block_allocator.h  # 专用块分配器
│       │   └── free_list_allocator.h        # 自由列表分配器
│       └── vulkan/
│           └── allocator.h          # Vulkan 内存分配器
└── src/
    ├── gpu_profiler/
    │   ├── gpu_profiler.cpp
    │   └── backend/
    │       └── vulkan.cpp
    └── gpu_allocator/
        ├── result.cpp
        ├── allocator.cpp
        ├── allocator/
        │   ├── dedicated_block_allocator.cpp
        │   └── free_list_allocator.cpp
        └── vulkan/
            └── allocator.cpp
```

## GPU Profiler

### 功能特性

- 每帧性能数据收集
- GPU 时间戳查询
- Scope 级别的性能追踪
- 多帧并行追踪支持 (默认 4 帧)

### 主要类型

- `GpuProfiler`: 全局性能分析器
- `ScopeId`: Scope 标识符
- `NanoSecond`: 纳秒时间单位
- `TimedFrame`: 包含性能数据的帧
- `VulkanProfilerFrame`: Vulkan 专用的帧分析器

### 使用示例

```cpp
#include "tekki/gpu_profiler/gpu_profiler.h"
#include "tekki/gpu_profiler/backend/vulkan.h"

// 获取全局 profiler
auto& profiler = tekki::Profiler();

// 开始一帧
profiler.BeginFrame();

// 创建 scope
auto scope_id = profiler.CreateScope("MyRenderPass");

// 在 Vulkan 中记录时间戳
VulkanProfilerFrame vulkan_frame(device, backend);
auto active_scope = vulkan_frame.BeginScope(device, cmd, scope_id);
// ... GPU 工作 ...
vulkan_frame.EndScope(device, cmd, active_scope);

// 结束一帧
profiler.EndFrame();

// 获取性能报告
if (auto* report = profiler.LastReport()) {
    for (const auto& scope : report->scopes) {
        std::cout << scope.name << ": " << scope.duration.Ms() << " ms\n";
    }
}
```

## GPU Allocator

### 功能特性

- Vulkan 内存管理
- 子分配支持 (减少 vkAllocateMemory 调用)
- 内存类型自动选择
- 专用分配支持
- 内存泄漏检测
- 调试信息和报告

### 主要类型

- `Allocator`: Vulkan 内存分配器
- `Allocation`: 内存分配句柄
- `AllocationCreateDesc`: 分配描述符
- `MemoryLocation`: 内存位置枚举 (GpuOnly, CpuToGpu, GpuToCpu)
- `AllocationScheme`: 分配方案 (Managed, Dedicated)

### 内存分配策略

#### 子分配器类型

1. **FreeListAllocator**:
   - 用于一般的内存分配
   - 支持多个分配共享同一个内存块
   - 使用最佳匹配 (best-fit) 或首次匹配 (first-fit) 策略
   - 处理粒度冲突 (granularity conflicts)

2. **DedicatedBlockAllocator**:
   - 用于专用分配
   - 每个分配使用独立的 VkDeviceMemory
   - 用于大型分配或需要专用内存的资源

### 使用示例

```cpp
#include "tekki/gpu_allocator/vulkan/allocator.h"

// 创建分配器
tekki::AllocatorCreateDesc allocator_desc{};
allocator_desc.instance = instance;
allocator_desc.device = device;
allocator_desc.physical_device = physical_device;
allocator_desc.buffer_device_address = true;
allocator_desc.debug_settings.log_memory_information = true;

tekki::Allocator allocator(allocator_desc);

// 创建 buffer
VkBufferCreateInfo buffer_info{};
buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
buffer_info.size = 1024;
buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

VkBuffer buffer;
vkCreateBuffer(device, &buffer_info, nullptr, &buffer);

VkMemoryRequirements requirements;
vkGetBufferMemoryRequirements(device, buffer, &requirements);

// 分配内存
tekki::AllocationCreateDesc alloc_desc{};
alloc_desc.name = "MyBuffer";
alloc_desc.requirements = requirements;
alloc_desc.location = tekki::MemoryLocation::CpuToGpu;
alloc_desc.linear = true;
alloc_desc.allocation_scheme = tekki::AllocationScheme::GpuAllocatorManaged;

auto allocation = allocator.Allocate(alloc_desc);

// 绑定内存
vkBindBufferMemory(device, buffer, allocation.Memory(), allocation.Offset());

// 使用映射内存 (如果是 CPU 可见的)
if (auto* mapped = allocation.MappedSlice()) {
    memcpy(mapped, data, size);
}

// 清理
vkDestroyBuffer(device, buffer, nullptr);
allocator.Free(std::move(allocation));
```

## 命名规范

遵循项目统一的命名风格:

- **文件名**: 小写下划线风格 (`gpu_profiler.h`, `free_list_allocator.cpp`)
- **类名**: 大驼峰 (`GpuProfiler`, `FreeListAllocator`)
- **方法名/函数名**: 大驼峰 (`BeginFrame`, `CreateScope`)
- **成员变量**: 小写下划线 + 后缀下划线 (`size_`, `allocated_`)
- **常量**: 全大写下划线 (`MAX_FRAMES_IN_FLIGHT`)

## 对应的原始 Rust 代码

原始 Rust 实现位于:

- `gpu-profiler/`: GPU profiler 原始代码
- `gpu-allocator/`: GPU allocator 原始代码

可以参考这些目录进行对比和验证翻译的正确性。

## 依赖项

### GPU Profiler

- Vulkan SDK
- spdlog (日志)

### GPU Allocator

- Vulkan SDK
- spdlog (日志)

## 特性对比

### 与 Rust 版本的差异

1. **错误处理**:
   - Rust: `Result<T, E>` 类型
   - C++: 异常 (`AllocationError`) 或返回值

2. **内存管理**:
   - Rust: 自动生命周期管理
   - C++: RAII + 移动语义

3. **并发**:
   - Rust: 编译期并发安全
   - C++: 使用 `std::atomic` 手动保证

4. **可选值**:
   - Rust: `Option<T>`
   - C++: `std::optional<T>`

## 调试功能

### GPU Allocator 调试

启用调试设置可以帮助追踪内存问题:

```cpp
tekki::AllocatorDebugSettings debug_settings{};
debug_settings.log_memory_information = true;  // 记录内存布局
debug_settings.log_leaks_on_shutdown = true;   // 记录内存泄漏
debug_settings.log_allocations = true;         // 记录每次分配
debug_settings.log_frees = true;               // 记录每次释放
```

### 内存泄漏报告

分配器在析构时会自动报告未释放的内存:

```
leak detected: {
    memory type: 0
    memory block: 2
    chunk: {
        chunk_id: 5,
        size: 0x1000,
        offset: 0x2000,
        allocation_type: Linear,
        name: MyBuffer
    }
}
```

## 性能考虑

### GPU Profiler

- 使用原子操作减少锁竞争
- 每帧限制最多 1024 个查询
- 查询结果缓存避免重复读取

### GPU Allocator

- 内存块大小可配置 (默认 256MB 设备, 64MB 主机)
- 支持内存块大小动态增长
- 使用最佳匹配减少内存碎片
- 相邻空闲块自动合并

## 后续工作

当前实现包含核心功能,但还有一些可选功能待完成:

1. Visualizer 支持 (可视化工具)
2. 完整的堆栈追踪支持
3. Puffin 集成 (性能分析工具)
4. 更多的单元测试

## 许可证

与主项目保持一致: MIT 或 Apache 2.0 双许可
