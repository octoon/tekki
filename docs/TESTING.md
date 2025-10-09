# Tekki 单元测试文档

## 概述

Tekki 项目使用 [Catch2](https://github.com/catchorg/Catch2) v3 作为单元测试框架。测试系统通过 CMake 选项控制，默认启用。

## 构建配置

### 启用/禁用测试

测试系统通过 `TEKKI_BUILD_TESTS` CMake 选项控制：

```bash
# 启用测试（默认）
cmake -DTEKKI_BUILD_TESTS=ON ..

# 禁用测试
cmake -DTEKKI_BUILD_TESTS=OFF ..
```

通过 Conan：

```bash
# 启用测试（默认）
conan install . -o build_tests=True

# 禁用测试
conan install . -o build_tests=False
```

### 构建测试

```bash
# Windows
Setup.bat

# 或手动构建
cmake --build build/debug --target tekki-test-core
cmake --build build/debug --target tekki-test-render-graph
cmake --build build/debug --target tekki-test-asset
cmake --build build/debug --target tekki-test-renderer
```

## 运行测试

### 运行所有测试

```bash
# 使用 CTest
cd build/debug
ctest --output-on-failure

# 使用自定义目标（并行运行）
cmake --build build/debug --target run-all-tests
```

### 运行特定模块测试

```bash
# 运行 core 模块测试
build/debug/bin/tekki-test-core.exe

# 运行 render graph 测试
build/debug/bin/tekki-test-render-graph.exe

# 运行 asset 模块测试
build/debug/bin/tekki-test-asset.exe

# 运行 renderer 模块测试
build/debug/bin/tekki-test-renderer.exe
```

### 运行特定测试用例

Catch2 支持通过名称或标签过滤测试：

```bash
# 运行特定测试用例
build/debug/bin/tekki-test-core.exe "Logging system initialization"

# 运行包含特定标签的测试
build/debug/bin/tekki-test-core.exe [log]

# 运行多个标签
build/debug/bin/tekki-test-renderer.exe [renderer][world]

# 排除特定标签
build/debug/bin/tekki-test-core.exe ~[slow]
```

### Catch2 命令行选项

```bash
# 列出所有测试
build/debug/bin/tekki-test-core.exe --list-tests

# 列出所有标签
build/debug/bin/tekki-test-core.exe --list-tags

# 详细输出
build/debug/bin/tekki-test-core.exe -s

# 显示成功的测试
build/debug/bin/tekki-test-core.exe -s -a
```

## 测试组织结构

```
tests/
├── CMakeLists.txt          # 测试构建配置
├── test_main.cpp           # Catch2 主入口
├── core/                   # 核心模块测试
│   ├── test_log.cpp        # 日志系统测试
│   ├── test_mmap.cpp       # 内存映射文件测试
│   └── test_math.cpp       # 数学库集成测试
├── render_graph/           # 渲染图模块测试
│   ├── test_resource.cpp   # 资源句柄测试
│   └── test_temporal.cpp   # 时间资源测试
├── asset/                  # 资源模块测试
│   ├── test_mesh.cpp       # 网格数据测试
│   └── test_image.cpp      # 图像加载测试
└── renderer/               # 渲染器模块测试
    ├── test_world_renderer.cpp  # 世界渲染器测试
    └── test_half_res.cpp   # 半分辨率工具测试
```

## 测试模块说明

### Core 模块测试 (tekki-test-core)

测试核心工具和类型系统：

- **test_log.cpp**: 日志系统初始化、不同日志级别、格式化字符串
- **test_mmap.cpp**: 内存映射文件打开/关闭、类型安全访问、RAII 语义、错误处理
- **test_math.cpp**: GLM 数学库集成、向量/矩阵操作、变换

**测试标签**: `[core]`, `[log]`, `[mmap]`, `[math]`

### Render Graph 模块测试 (tekki-test-render-graph)

测试渲染图资源管理系统：

- **test_resource.cpp**: 资源句柄创建和比较、导出句柄、资源视图（SRV/UAV/RT）
- **test_temporal.cpp**: PingPong 和 TripleBuffer 时间资源、历史帧管理、实际使用模式（TAA、运动向量）

**测试标签**: `[render_graph]`, `[resource]`, `[temporal]`

### Asset 模块测试 (tekki-test-asset)

测试资源加载和处理系统：

- **test_mesh.cpp**: 三角网格数据结构、材质系统、打包网格、网格句柄
- **test_image.cpp**: 图像格式检测、图像数据结构、Mip 级别计算、通道交换、压缩格式

**测试标签**: `[asset]`, `[mesh]`, `[image]`

### Renderer 模块测试 (tekki-test-renderer)

测试渲染器核心功能：

- **test_world_renderer.cpp**: 网格/实例句柄、网格实例变换、G-buffer 结构、曝光状态、渲染模式、Bindless 图像句柄
- **test_half_res.cpp**: 半分辨率计算、过滤模式

**测试标签**: `[renderer]`, `[world]`, `[half_res]`

## 编写新测试

### 基本测试结构

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test description", "[module][tag]") {
    SECTION("Test section 1") {
        // Arrange
        int value = 42;

        // Act & Assert
        REQUIRE(value == 42);
    }

    SECTION("Test section 2") {
        // Another test scenario
        REQUIRE(true);
    }
}
```

### 浮点数比较

```cpp
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("Float comparison", "[math]") {
    float value = 3.14f;
    REQUIRE_THAT(value, Catch::Matchers::WithinRel(3.14f, 0.001f));
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(3.14f, 0.01f));
}
```

### 异常测试

```cpp
TEST_CASE("Exception handling", "[error]") {
    REQUIRE_THROWS_AS(throw_function(), std::runtime_error);
    REQUIRE_NOTHROW(safe_function());
}
```

### 添加测试到构建系统

1. 在相应的 `tests/` 子目录中创建新的 `.cpp` 文件
2. 更新 `tests/CMakeLists.txt`，将文件添加到相应的测试可执行文件

```cmake
add_executable(tekki-test-core
    test_main.cpp
    core/test_log.cpp
    core/test_mmap.cpp
    core/test_math.cpp
    core/test_new_feature.cpp  # 添加新测试
)
```

## 测试覆盖率

当前测试覆盖的主要功能：

### ✅ 已覆盖

- **Core 模块**:
  - 日志系统初始化和使用
  - 内存映射文件操作
  - GLM 数学库基础功能

- **Render Graph 模块**:
  - 资源句柄系统
  - 时间资源管理（PingPong 和 TripleBuffer）
  - 资源视图类型

- **Asset 模块**:
  - 网格数据结构和操作
  - 图像格式检测
  - 材质系统基础

- **Renderer 模块**:
  - WorldRenderer 数据结构
  - 曝光控制系统
  - 半分辨率工具函数

### ⚠️ 待覆盖（未来扩展）

- Vulkan 后端集成测试（需要 Vulkan 环境）
- 渲染图执行测试（需要 GPU）
- 完整的渲染管线测试
- glTF 加载器集成测试
- 着色器编译测试

## 持续集成 (CI)

测试系统设计可以集成到 CI/CD 流程中：

```yaml
# GitHub Actions 示例
- name: Build and Test
  run: |
    cmake --build build --config Debug
    cd build/Debug
    ctest --output-on-failure
```

## 最佳实践

1. **测试命名**: 使用描述性的测试用例名称
2. **测试标签**: 为测试添加合适的标签以便过滤
3. **测试隔离**: 每个测试应该独立，不依赖其他测试的状态
4. **快速测试**: 保持单元测试快速运行（< 1秒）
5. **清晰断言**: 使用明确的断言消息
6. **测试覆盖**: 测试正常情况和边界情况

## 调试测试

### Visual Studio

1. 右键点击测试可执行文件项目
2. 选择 "Set as Startup Project"
3. 设置断点
4. F5 开始调试

### 命令行调试

```bash
# 运行特定测试并显示详细输出
build/debug/bin/tekki-test-core.exe "Test name" -s -a

# 在调试器中运行
gdb build/debug/bin/tekki-test-core.exe
```

## 性能测试

Catch2 支持基准测试（需要启用）：

```cpp
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Benchmark example", "[!benchmark]") {
    BENCHMARK("Function name") {
        return expensive_function();
    };
}
```

## 参考资料

- [Catch2 文档](https://github.com/catchorg/Catch2/tree/devel/docs)
- [CMake CTest 文档](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Tekki 开发文档](./DEVELOPMENT.md)
