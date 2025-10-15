# NGX DLSS 枚举命名约定说明

## 为什么枚举值有重复的前缀？

在 `ngx_dlss.h` 中，你会看到类似这样的枚举定义：

```c
typedef enum NVSDK_NGX_Result {
    NVSDK_NGX_Result_NVSDK_NGX_Result_Success = 0,  // 注意重复的前缀
    NVSDK_NGX_Result_NVSDK_NGX_Result_Fail = 1
} NVSDK_NGX_Result;
```

这看起来很奇怪，但这是**故意的设计**，原因如下：

## Bindgen 的命名约定

原始的 kajiya 项目使用 Rust 编写，通过 **bindgen** 工具从 NVIDIA NGX SDK 的 C 头文件自动生成 Rust FFI 绑定。

Bindgen 的行为：
1. C 头文件中定义：`NVSDK_NGX_Result_Success`
2. Bindgen 生成 Rust 绑定时，为避免命名冲突，会添加类型名作为前缀
3. 最终生成：`NVSDK_NGX_Result_NVSDK_NGX_Result_Success`

## 在 Rust 代码中的使用

查看 `kajiya/crates/lib/kajiya/src/renderers/dlss.rs`：

```rust
// 第 18 行
assert_eq!(NVSDK_NGX_Result_NVSDK_NGX_Result_Success, $($t)*)

// 第 41 行
NVSDK_NGX_Result_NVSDK_NGX_Result_Success

// 第 108-111 行
NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_MaxQuality,
NVSDK_NGX_PerfQuality_Value_NVSDK_NGX_PerfQuality_Value_Balanced,
// ... 等等
```

所有这些使用都是 bindgen 自动生成的命名格式。

## 我们的 C++ 实现

为了保持与原始 Rust 代码的兼容性，我们的 C++ stub 头文件 `ngx_dlss.h` **必须使用相同的命名约定**。

这样做的好处：
1. **与 Rust 源代码保持一致**：便于对照翻译
2. **避免混淆**：清楚表明这是 bindgen 风格的绑定
3. **未来兼容**：如果需要与实际的 NGX SDK 集成，保持命名一致性

## 参考

- 原始 Rust 代码：`kajiya/crates/lib/kajiya/src/renderers/dlss.rs`
- Bindgen 配置：`kajiya/crates/lib/ngx_dlss/build.rs`
- C++ Stub 实现：`src/third_party/ngx_dlss_stubs.cpp`
