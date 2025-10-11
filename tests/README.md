# Tekki 单元测试

本目录包含tekki渲染器的单元测试套件，使用Catch2测试框架。

## 测试结构

```
tests/
├── core/                   # 核心工具类测试
│   └── test_result.cpp     # Result类型测试
├── backend/                # 后端/Vulkan组件测试
│   ├── test_bytes.cpp      # 字节转换测试
│   ├── test_chunky_list.cpp # ChunkyList测试
│   ├── test_dynamic_constants.cpp
│   ├── test_file.cpp       # 文件操作测试
│   └── test_transient_resource_cache.cpp
├── render_graph/           # 渲染图测试
│   ├── test_graph.cpp      # 渲染图核心测试
│   └── test_temporal.cpp   # 时序资源测试
├── asset/                  # 资产加载测试
│   ├── test_image.cpp      # 图像加载测试
│   └── test_mesh.cpp       # 网格加载测试
├── renderer/               # 渲染器测试
│   ├── test_camera.cpp     # 相机测试
│   ├── test_math.cpp       # 数学库测试
│   └── test_buffer_builder.cpp # 缓冲区构建器测试
├── CMakeLists.txt          # 测试构建配置
└── test_main.cpp           # 测试主入口
```

## 构建测试

测试会随主项目一起构建（如果启用了TEKKI_BUILD_TESTS选项）：

```bash
# Windows
Setup.bat

# 或手动构建
conan install . --profile:host conan/profiles/windows-msvc-debug --build=missing --output-folder=build/debug
cmake --preset conan-default
cmake --build --preset conan-debug
```

## 运行测试

构建完成后，可以运行测试可执行文件：

```bash
# 运行所有测试
build\debug\bin\tekki-tests.exe

# 运行特定模块的测试
build\debug\bin\tekki-tests.exe [core]
build\debug\bin\tekki-tests.exe [backend]
build\debug\bin\tekki-tests.exe [render_graph]
build\debug\bin\tekki-tests.exe [asset]
build\debug\bin\tekki-tests.exe [renderer]

# 运行特定测试用例
build\debug\bin\tekki-tests.exe "Result basic operations"

# 显示详细输出
build\debug\bin\tekki-tests.exe -s

# 列出所有测试
build\debug\bin\tekki-tests.exe --list-tests
```

## 使用CTest

也可以通过CTest运行测试：

```bash
cd build\debug
ctest --output-on-failure

# 运行特定测试
ctest -R core
ctest -R backend

# 并行运行测试
ctest -j 8
```

## 测试覆盖范围

### 核心模块测试 (core/)
- ✅ Result类型（Rust风格的错误处理）
- ✅ Error类和上下文管理
- ✅ Result的各种操作（Map, Unwrap, Expect等）

### 后端模块测试 (backend/)
- ✅ 字节转换工具（IntoByteVec, AsByteSlice）
- ✅ ChunkyList（临时列表数据结构）
- ✅ DynamicConstants（动态常量缓冲）
- ✅ 文件系统操作（LoadFile, VirtualFileSystem）
- ⚠️  TransientResourceCache（需要Vulkan初始化，在集成测试中）

### 渲染图模块测试 (render_graph/)
- ⚠️  RenderGraph（需要Vulkan设备，在集成测试中）
- ⚠️  Temporal资源（需要渲染图设置，在集成测试中）

### 资产模块测试 (asset/)
- ✅ 图像格式检测
- ✅ 网格数据结构
- ⚠️  实际文件加载（需要测试资产，在集成测试中）

### 渲染器模块测试 (renderer/)
- ✅ 数学工具函数
- ✅ GLM类型和操作
- ⚠️  Camera（需要完整初始化，在集成测试中）
- ⚠️  BufferBuilder（需要Vulkan缓冲，在集成测试中）

## 注意事项

1. **集成测试**: 某些组件（如RenderGraph、Camera、TransientResourceCache）需要完整的Vulkan初始化。这些组件的完整测试在集成测试套件中进行。

2. **测试数据**: 某些测试（如图像和网格加载）需要测试资产文件。如果测试数据不存在，这些测试会被跳过。

3. **Vulkan要求**: 涉及GPU的测试需要有效的Vulkan设备和驱动程序。

## 添加新测试

要添加新的测试文件：

1. 在相应目录下创建 `test_*.cpp` 文件
2. 在 `tests/CMakeLists.txt` 中添加新文件到 `tekki-tests` 目标
3. 使用Catch2的TEST_CASE宏编写测试
4. 重新构建项目

示例：
```cpp
#include <catch2/catch_test_macros.hpp>
#include <tekki/your_module/your_class.h>

TEST_CASE("Your test description", "[module][tag]") {
    SECTION("Section description") {
        // 测试代码
        REQUIRE(1 + 1 == 2);
    }
}
```

## 测试标签

使用标签来组织和过滤测试：
- `[core]` - 核心工具类测试
- `[backend]` - 后端/Vulkan测试
- `[render_graph]` - 渲染图测试
- `[asset]` - 资产加载测试
- `[renderer]` - 渲染器测试

运行特定标签的测试：
```bash
build\debug\bin\tekki-tests.exe [core]
```

## 持续集成

测试可以在CI/CD管道中自动运行：

```yaml
# GitHub Actions 示例
- name: Run tests
  run: |
    cd build/debug
    ctest --output-on-failure
```

## 参考资料

- [Catch2文档](https://github.com/catchorg/Catch2/tree/devel/docs)
- [CMake CTest文档](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Conan包管理器](https://docs.conan.io/)
