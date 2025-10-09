# Kajiya Rust to C++ Translation Tool

自动化工具，用于将 kajiya 的 Rust 源码翻译成 C++ 代码。

## 特性

- **智能依赖分析**: 自动分析 Rust 模块间的 `use` 依赖关系
- **拓扑排序**: 根据依赖关系确定翻译顺序，从依赖最少的模块开始
- **上下文感知翻译**: 翻译时将已翻译的依赖作为上下文传递给 GPT
- **标准化输出**: 自动生成配对的 `.h` 和 `.cpp` 文件
- **命名空间管理**: 自动设置正确的 `tekki::` 命名空间
- **进度追踪**: 支持中断恢复，不会重复翻译已完成的模块
- **增量翻译**: 可以随时停止和恢复

## 安装依赖

```bash
pip install openai
```

## 使用方法

### 基本用法

```bash
python translate_kajiya.py \
  --base-url "https://your-api-endpoint.com/v1" \
  --api-key "your-api-key" \
  --model "gpt-5-high"
```

### 完整参数

```bash
python translate_kajiya.py \
  --base-url <API_BASE_URL>          # OpenAI API 基础 URL (必需)
  --api-key <API_KEY>                # OpenAI API 密钥 (必需)
  --model <MODEL_NAME>               # 模型名称 (默认: gpt-5-high)
  --kajiya-root <PATH>               # kajiya 源码路径 (默认: ./kajiya)
  --output-root <PATH>               # 输出根目录 (默认: .)
  --progress-file <PATH>             # 进度文件路径 (默认: .translation_progress.json)
```

### 实际示例

```bash
# 使用默认设置
python translate_kajiya.py \
  --base-url "https://api.openai.com/v1" \
  --api-key "sk-xxxxx"

# 指定输出目录
python translate_kajiya.py \
  --base-url "https://api.openai.com/v1" \
  --api-key "sk-xxxxx" \
  --output-root "C:/projects/tekki"

# 使用自定义模型
python translate_kajiya.py \
  --base-url "https://custom-endpoint.com/v1" \
  --api-key "your-key" \
  --model "gpt-4-turbo"
```

## 工作原理

### 1. 扫描阶段

脚本会扫描 `kajiya/crates/lib/` 下的所有 Rust 源文件：

- `kajiya` → `tekki::renderer`
- `kajiya-backend` → `tekki::backend`
- `kajiya-rg` → `tekki::render_graph`
- `kajiya-asset` → `tekki::asset`

### 2. 依赖分析

分析每个模块的 `use` 语句，构建依赖关系图：

```rust
use kajiya_backend::Device;  // 依赖 kajiya-backend
use crate::camera::Camera;    // 依赖同一 crate 的 camera 模块
```

### 3. 拓扑排序

使用 Kahn 算法进行拓扑排序，确保：

- 先翻译被依赖的模块
- 后翻译依赖其他模块的模块
- 检测循环依赖

### 4. 增量翻译

按拓扑顺序翻译，每个模块：

1. 收集已翻译的依赖作为上下文
2. 调用 GPT API 生成 `.h` 文件
3. 调用 GPT API 生成 `.cpp` 文件
4. 保存文件到正确的目录
5. 更新进度文件

### 5. 文件映射

Rust 文件路径 → C++ 文件路径：

```
kajiya/crates/lib/kajiya/src/renderers/ibl.rs
  → include/tekki/renderer/renderers/ibl.h
  → src/renderer/renderers/ibl.cpp

kajiya/crates/lib/kajiya-backend/src/vulkan/device.rs
  → include/tekki/backend/vulkan/device.h
  → src/backend/vulkan/device.cpp
```

## 输出结构

```
tekki/
  include/
    tekki/
      renderer/         # kajiya
      backend/          # kajiya-backend
      render_graph/     # kajiya-rg
      asset/            # kajiya-asset
  src/
    renderer/
    backend/
    render_graph/
    asset/
  .translation_progress.json  # 进度追踪文件
```

## 命名约定

脚本会自动应用 tekki 项目的命名规范：

- **类名**: `PascalCase`
- **方法名**: `PascalCase`
- **文件名**: `snake_case`
- **命名空间**: `tekki::{subsystem}::{submodule}`

## 翻译提示

脚本会向 GPT 提供以下指导：

- 使用 C++20 特性
- 使用 `tekki::core::Result` 进行错误处理
- 类型映射:
  - `Vec<T>` → `std::vector<T>`
  - `Arc<T>` → `std::shared_ptr<T>`
  - `Option<T>` → `std::optional<T>`
  - `&str` / `String` → `std::string` / `std::string_view`
- 包含必要的头文件
- 使用头文件保护 (`#pragma once`)

## 进度管理

### 查看进度

```bash
cat .translation_progress.json
```

### 重新翻译某个模块

编辑 `.translation_progress.json`，将对应模块的状态改为 `false`：

```json
{
  "kajiya::renderers::ibl": false
}
```

### 清除进度重新开始

```bash
rm .translation_progress.json
```

## 中断和恢复

- 按 `Ctrl+C` 可以随时中断
- 已翻译的模块会保存在进度文件中
- 再次运行脚本会自动跳过已完成的模块

## 注意事项

1. **API 成本**: 翻译大量文件会消耗 API 配额，建议先小规模测试
2. **人工审查**: GPT 翻译结果需要人工审查和调整
3. **依赖检测**: 仅检测显式的 `use` 语句，可能遗漏隐式依赖
4. **外部依赖**: 不会翻译标准库和第三方库的依赖
5. **循环依赖**: 如果检测到循环依赖，会发出警告但继续翻译

## 示例输出

```
Scanning kajiya crates...
  Found: kajiya::camera
  Found: kajiya::renderers::ibl
  Found: kajiya-backend::vulkan::device
Total modules found: 45

Analyzing dependencies...
  camera depends on: {kajiya_backend::Device}
  renderers::ibl depends on: {camera, kajiya_rg::RenderGraph}

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

...

✓ Translation complete!
```

## 故障排除

### 问题: "Module not found in scanned modules"

**原因**: 依赖引用了未被扫描到的模块

**解决**: 检查 `use` 语句是否正确，或手动添加模块到扫描列表

### 问题: API 调用失败

**原因**: API 密钥无效或网络问题

**解决**:
- 检查 `--api-key` 和 `--base-url`
- 检查网络连接
- 检查 API 配额

### 问题: 翻译质量不佳

**原因**: 缺少上下文或提示词不够详细

**解决**:
- 调整 `_translate_to_header` 和 `_translate_to_source` 中的提示词
- 增加依赖上下文的长度
- 使用更强大的模型

## 扩展开发

### 添加新的 crate 映射

编辑 `DependencyAnalyzer.crate_map`:

```python
self.crate_map = {
    "kajiya": "kajiya",
    "your_new_crate": "your-crate-name"
}
```

### 自定义命名空间

编辑 `GPTTranslator._get_namespace`:

```python
crate_to_ns = {
    "kajiya": "renderer",
    "your_crate": "your_namespace"
}
```

### 修改翻译提示

编辑 `GPTTranslator._translate_to_header` 和 `_translate_to_source` 中的 `prompt` 变量。

## License

与 tekki 项目相同，采用 MIT 和 Apache 2.0 双重许可。
