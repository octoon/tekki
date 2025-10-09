# Kajiya 翻译工具快速开始

## 快速使用

### Windows

```batch
REM 基本用法
run_translation.bat "https://api.openai.com/v1" "your-api-key"

REM 使用自定义模型
run_translation.bat "https://api.openai.com/v1" "your-api-key" "gpt-4-turbo"
```

### Linux/Mac

```bash
# 基本用法
./run_translation.sh "https://api.openai.com/v1" "your-api-key"

# 使用自定义模型
./run_translation.sh "https://api.openai.com/v1" "your-api-key" "gpt-4-turbo"
```

### Python 直接调用

```bash
python translate_kajiya.py \
  --base-url "https://api.openai.com/v1" \
  --api-key "your-api-key" \
  --model "gpt-5-high"
```

## 预期输出

运行后会看到类似输出：

```
========================================
Kajiya Rust to C++ Translation Tool
========================================

Base URL: https://api.openai.com/v1
Model: gpt-5-high
Output: C:\Users\jstzw\Documents\GitHub\tekki
Progress File: C:\Users\jstzw\Documents\GitHub\tekki\.translation_progress.json

Starting translation...
Press Ctrl+C to interrupt (progress will be saved)

Scanning kajiya crates...
  Found: kajiya::bindless_descriptor_set
  Found: kajiya::buffer_builder
  Found: kajiya::camera
  Found: kajiya::renderers::ibl
  Found: kajiya::renderers::ircache
  ... (更多模块)
Total modules found: 45

Analyzing dependencies...
  bindless_descriptor_set depends on: {kajiya_backend::Device}
  camera depends on: {kajiya_backend::Device}
  renderers::ibl depends on: {camera, kajiya_rg::RenderGraph}
  ... (更多依赖)

Performing topological sort...
Translation order determined: 45 modules

Starting translation of 45 modules...
Output directory: C:\Users\jstzw\Documents\GitHub\tekki

[1/45] Translating kajiya-backend::vulkan::device...
  Written: include/tekki/backend/vulkan/device.h
  Written: src/backend/vulkan/device.cpp
[1/45] ✓ kajiya-backend::vulkan::device

[2/45] Translating kajiya::camera...
  Written: include/tekki/renderer/camera.h
  Written: src/renderer/camera.cpp
[2/45] ✓ kajiya::camera

... (继续翻译)

[45/45] ✓ kajiya::world_renderer

✓ Translation complete!

========================================
Translation completed successfully!
========================================
```

## 生成的文件结构

```
tekki/
├── include/
│   └── tekki/
│       ├── renderer/              # kajiya crate
│       │   ├── camera.h
│       │   ├── bindless_descriptor_set.h
│       │   └── renderers/
│       │       ├── ibl.h
│       │       ├── ircache.h
│       │       └── lighting.h
│       ├── backend/               # kajiya-backend crate
│       │   └── vulkan/
│       │       └── device.h
│       ├── render_graph/          # kajiya-rg crate
│       └── asset/                 # kajiya-asset crate
├── src/
│   ├── renderer/
│   │   ├── camera.cpp
│   │   └── renderers/
│   │       └── ibl.cpp
│   ├── backend/
│   ├── render_graph/
│   └── asset/
└── .translation_progress.json    # 进度追踪
```

## 示例: 查看生成的代码

翻译后的文件示例：

**include/tekki/renderer/camera.h**
```cpp
#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace tekki::renderer {

class Camera {
public:
    Camera();
    ~Camera();

    void SetPosition(const glm::vec3& position);
    void SetRotation(const glm::quat& rotation);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tekki::renderer
```

**src/renderer/camera.cpp**
```cpp
#include "tekki/renderer/camera.h"

namespace tekki::renderer {

struct Camera::Impl {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float fov{60.0f};
    float aspect{16.0f / 9.0f};
};

Camera::Camera() : impl_(std::make_unique<Impl>()) {}
Camera::~Camera() = default;

void Camera::SetPosition(const glm::vec3& position) {
    impl_->position = position;
}

void Camera::SetRotation(const glm::quat& rotation) {
    impl_->rotation = rotation;
}

glm::mat4 Camera::GetViewMatrix() const {
    // Implementation...
}

glm::mat4 Camera::GetProjectionMatrix() const {
    // Implementation...
}

} // namespace tekki::renderer
```

## 中断和恢复

如果需要中断翻译（例如检查 API 成本）：

1. 按 `Ctrl+C` 停止
2. 进度会自动保存到 `.translation_progress.json`
3. 重新运行相同命令会从上次停止的地方继续

**查看进度**:
```bash
# Windows
type .translation_progress.json

# Linux/Mac
cat .translation_progress.json
```

**输出示例**:
```json
{
  "kajiya-backend::vulkan::device": true,
  "kajiya::camera": true,
  "kajiya::renderers::ibl": false
}
```

## 重新翻译特定模块

如果对某个模块的翻译不满意：

1. 编辑 `.translation_progress.json`
2. 将该模块的状态改为 `false`
3. 重新运行脚本

```json
{
  "kajiya::camera": false  // 将重新翻译
}
```

## 完全重新开始

```bash
# Windows
del .translation_progress.json

# Linux/Mac
rm .translation_progress.json
```

然后重新运行翻译脚本。

## 下一步

1. **审查代码**: GPT 生成的代码需要人工审查
2. **修复编译错误**: 调整 include 路径、类型转换等
3. **添加到 CMakeLists.txt**: 将新文件添加到构建系统
4. **运行测试**: 确保翻译的代码行为正确

详细文档见: [TRANSLATION_TOOL.md](TRANSLATION_TOOL.md)
