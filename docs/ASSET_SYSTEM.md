# Tekki Asset System

完整的资源加载和处理系统，从 kajiya-asset 和 kajiya-asset-pipe Rust crates 翻译而来。

## 功能特性

### 图像加载 (`image.h`)
- 支持多种格式: PNG, JPEG, BMP, TGA, HDR, EXR
- DDS 压缩纹理支持 (BC1, BC3, BC5, BC7)
- BC5/BC7 纹理压缩 (占位实现)
- Mipmap 自动生成
- Channel swizzling
- SRGB/Linear 色彩空间

### 网格加载 (`mesh.h`)
- glTF 2.0 支持
- 完整的 PBR 材质
- 切线空间计算
- 顶点数据打包优化
- 材质贴图管理

### glTF 导入 (`gltf_importer.h`)
- URI 解析 (file://, data:, relative paths)
- Base64 解码
- Buffer 和 Image 加载
- 纹理变换支持

### 资源管道 (`asset_pipeline.h`)
- 异步资源加载
- 资源缓存系统
- 多线程图像处理
- FlatBuffer 风格序列化
- 离线资源处理工具

## 架构

```
tekki::asset/
├── image.h/.cpp           # 图像加载和处理
├── image_process.cpp      # 图像压缩和转换
├── mesh.h/.cpp            # 网格数据结构
├── gltf_importer.h/.cpp   # glTF 文件导入
├── gltf_loader.cpp        # glTF 场景加载
├── gltf_loader_process.cpp # glTF 节点处理
├── tangent_calc.cpp       # 切线计算 (mikktspace)
├── serialization.cpp      # 序列化/反序列化
└── asset_pipeline.h/.cpp  # 资源管道和缓存
```

## 使用示例

### 加载 glTF 模型

```cpp
#include <tekki/asset/mesh.h>

using namespace tekki::asset;

// 加载 glTF 场景
GltfLoadParams params;
params.path = "models/scene.gltf";
params.scale = 1.0f;
params.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

GltfLoader loader;
auto meshResult = loader.Load(params);

if (meshResult) {
    TriangleMesh& mesh = *meshResult;
    // 使用网格数据...
}
```

### 打包网格用于 GPU

```cpp
// 打包网格
PackedTriMesh packed = PackTriangleMesh(mesh);

// 序列化保存
std::vector<uint8_t> data = SerializePackedMesh(packed);
std::ofstream file("mesh.bin", std::ios::binary);
file.write(reinterpret_cast<const char*>(data.data()), data.size());
```

### 加载图像

```cpp
#include <tekki/asset/image.h>

// 从文件加载
ImageSource source = ImageSource::FromFile("texture.png");

auto rawResult = ImageLoader::Load(source);
if (rawResult) {
    RawImage& raw = *rawResult;
    // 处理原始图像...
}
```

### 创建 GPU 图像

```cpp
// 配置参数
TexParams params;
params.gamma = TexGamma::Srgb;
params.useMips = true;
params.compression = TexCompressionMode::Rgba;

// 创建 GPU 图像
GpuImageCreator creator(params);
auto gpuResult = creator.Create(raw);

if (gpuResult) {
    GpuImageProto& gpu = *gpuResult;
    // gpu.format, gpu.extent, gpu.mips
}
```

### 使用资源缓存

```cpp
#include <tekki/asset/asset_pipeline.h>

// 异步加载网格
auto future = AssetCache::Instance().LoadMesh(params);

// 等待加载完成
auto meshResult = future.get();
if (meshResult) {
    std::shared_ptr<PackedTriMesh> mesh = *meshResult;
    // 使用缓存的网格...
}
```

### 离线资源处理

```cpp
// 处理网格资产到缓存目录
MeshAssetProcessParams processParams;
processParams.path = "models/scene.gltf";
processParams.outputName = "scene";
processParams.scale = 1.0f;

AssetProcessor processor;
auto result = processor.ProcessMeshAsset(processParams);

// 生成文件:
// cache/scene.mesh
// cache/XXXXXXXX.image (for each unique texture)
```

## 依赖

- **TinyGLTF**: glTF 2.0 文件解析
- **stb_image**: 标准图像格式加载
- **stb_image_resize**: 图像缩放
- **OpenEXR**: HDR 图像支持
- **GLM**: 数学库
- **fmt**: 字符串格式化

## 与 Rust 版本的差异

1. **异步处理**: 使用 C++ `std::future` 和线程池代替 Rust 的 async/await
2. **错误处理**: 使用自定义 `Result<T>` 类型代替 Rust 的 `Result<T, E>`
3. **序列化**: 保留了 FlatBuffer 风格的序列化，但实现细节不同
4. **BC 压缩**: 暂时使用占位实现，需要集成 intel_tex_2 或类似库
5. **Lazy evaluation**: 暂未实现完整的 turbosloth 风格 lazy 系统

## TODO

- [ ] 实现完整的 BC5/BC7 压缩 (集成 intel_tex_2)
- [ ] 添加更多图像格式支持
- [ ] 实现完整的 mipmap 生成 (gamma-correct)
- [ ] 优化内存分配和拷贝
- [ ] 添加单元测试
- [ ] 性能优化和基准测试
- [ ] 支持更多 glTF 扩展

## 参考

- 原始 Rust 实现: `kajiya/crates/lib/kajiya-asset/`
- 原始资源管道: `kajiya/crates/lib/kajiya-asset-pipe/`
