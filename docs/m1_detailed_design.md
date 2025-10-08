# M1 详细设计与实施计划

## 1. 阶段目标
- 建立 `tekki` C++ 项目的最小可编译骨架。
- 明确依赖获取、构建流程和基础子系统接口。
- 交付可运行的 GLFW + Vulkan 验证程序，为后续渲染器迁移打底。

## 2. 构建与依赖方案
- **构建工具链**：CMake + Conan + Ninja。
- **配置文件**：
  - `conan/profiles/linux-gcc-{debug,release}`、`windows-msvc-{debug,release}` 提供参考 profile。
  - `Setup.sh` / `Setup.bat` 封装常用的 Conan 安装与 CMake 预设生成步骤。
  - `CMakePresets.json` 绑定 Conan 生成的 toolchain，统一 `build/debug`、`build/release` 输出路径。
- **第三方依赖**：以 Conan 官方包为主（GLFW、GLM、spdlog、imgui、Vulkan SDK、nlohmann_json 等）。缺失库通过自建 recipe 或 `external/` 子模块补充。

## 3. Core 模块接口
### 3.1 Logging (`core/log.hpp/.cpp`)
- `void init(spdlog::level::level_enum level)`：初始化日志。
- `std::shared_ptr<spdlog::logger> get_logger()`：返回单例，支持懒初始化。
- 宏 `TEKKI_LOG_*` 提供统一日志入口。

### 3.2 Time (`core/time.hpp/.cpp`)
- `double now_seconds()`：获取当前时间戳。
- `std::string format_timestamp(Clock::time_point)`：格式化为 `HH:MM:SS.mmm`。
- `FrameTimer`：`tick()` 返回帧间隔，`total_seconds()` 累积时间。
- `ScopedTimer`：作用域计时，析构时输出耗时日志。

### 3.3 Config (`core/config.hpp/.cpp`)
- `AppConfig`：窗口参数、日志级别、验证层开关、Bootstrap 帧数。
- `AppConfig load_from_file(path)`：从 `data/config/viewer.json` 读取 JSON；若文件缺失/异常，回退默认值并输出错误到 `std::cerr`。

## 4. Viewer 启动流程
1. 读取配置并初始化日志（`tekki::config::load_from_file` → `tekki::log::init`）。
2. 初始化 GLFW，创建无 API 窗口。
3. 查询必需扩展，创建 Vulkan Instance（可选启用校验层）。
4. 使用 `FrameTimer` 驱动主循环，定期打印帧时长；支持 `bootstrap_frames` 退出。
5. 清理 Vulkan/GLFW 资源，记录日志。

> 该流程定义在 `src/viewer/main.cpp`，为未来接入 render graph/renderer 奠定基础结构。

## 5. 构建流程
1. 本地开发：
   ```bash
   conan install . --profile:host conan/profiles/linux-gcc-debug --profile:build conan/profiles/linux-gcc-debug --build=missing --output-folder=build/debug
   cmake --preset conan-debug
   cmake --build --preset conan-debug
   ```
2. 未来添加单元测试后，可在 `ctest --preset conan-debug` 中覆盖，并视情况接入自动化流程。

## 6. 风险与应对
- **GLFW/系统依赖缺失**：Linux 环境需通过 `apt-get` 等方式安装 X11/Wayland 相关开发包并保持脚本更新。
- **Vulkan SDK 版本差异**：统一由 Conan 提供 headers/loader，避免系统版本不一致。
- **配置解析缺失校验**：`AppConfig` 当前只做基础验证；后续需要引入 Schema/更完善的错误报告机制。
- **日志依赖初始化顺序**：配置解析与日志初始化解耦，避免尚未初始化 logger 即输出。

## 7. 后续扩展点
- 在 Core 层增加 `Error`, `Result` 封装以及断言宏统一处理。
- 引入任务系统与文件监听（`efsw`）以支持热重载。
- 扩展 Viewer：加入命令行参数、UI、渲染图绑定。
- 为 Time/Config 添加单元测试，确保跨平台一致。

该文档作为 M1 阶段实施依据，后续评审可根据实际工程需求调整或细化。 
