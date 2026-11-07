# vsglab

按 [vsgExamples](https://github.com/vsg-dev/vsgExamples) 的顺序学习 VulkanSceneGraph 的试验场：
每个原版示例移植为一个"模板"，在 Qt 界面上可以随时创建运行实例，**同一个示例可以重复创建多个**，
每个实例有独立的参数与视口（标签页），互不影响。原版的命令行参数改为界面控件。

## 环境要求

- Windows（Linux/macOS 的句柄适配代码在 `src/host/platform/` 下，未在 CI 验证）
- Visual Studio 2026（vcpkg 的 glslang 由 MSVC 14.51 编译，链接工具链不得低于它）
- 环境变量：
  - `QTDIR` — Qt6 MSVC 目录（如 `F:/dev/qt6/6.8.3/msvc2022_64`）
  - `VCPKG_ROOT` — vcpkg 根目录（定位 `installed/x64-windows/share/glslang`，供 vsgConfig 使用）
  - `VSG_WORKSPACE` — VSG 生态根目录，需包含：
    `VulkanSceneGraph/install`、`vsgXchange/install`（可选）、`vsgImGui/install`（可选）、`osg2vsg/install`（vsgXchange 依赖）
- 仓库根目录下需有 `vsgExamples/`（示例数据与原版源码；已被 .gitignore 忽略）

以上路径均可被 CMake 缓存变量覆盖：`VSGLAB_QT_ROOT` / `vsg_DIR` / `glslang_DIR` / `osg2vsg_DIR` / `vsgXchange_DIR` / `vsgImGui_DIR`。

## 构建

```bat
cmake --preset msvc
cmake --build --preset relwithdebinfo   :: 或 --preset debug
```

产物：`build/bin/<配置>/vsglab.exe`（构建后自动 windeployqt）。

## 使用

- 左侧清单**双击**示例（或选中后点工具栏 New Instance）创建一个实例标签页，可重复创建
- 每个实例的参数独立、从默认值开始；窗口参数组为 Restart 类（修改后自动重启示例）
- Console 类示例（如 core/maths，无窗口）运行一次，结果显示在其标签页内
- 底部输出区显示 vsg 日志与 `std::cout`，带实例标签（如 `[vsghelloworld #2]`）

## 代码结构

```
src/framework/   示例接口与参数系统（不依赖 Qt）
                 example.h            Example 基类（Window / Console 两种形态）
                 example_registry.h   ExampleDescriptor 清单（只登记"如何创建"）
                 param_set.h          类型化参数句柄（Live / Restart）
                 window_params.h      公共窗口参数（对应 WindowTraits 命令行选项）
src/host/        运行宿主（Qt 层，但不包含界面布局）
                 vsg_host.h           生命周期、帧循环、异常隔离；隐藏时暂停出帧
                 native_surface.h     原生子窗口 + Qt→vsg 事件桥（键鼠/HiDPI）
                 platform/            各平台原生句柄写入（_win32 / _x11 / _macos）
                 log_bridge.h         vsg::Logger → 界面，thread_local 实例标签
                 key_map.h            Qt 键码 → vsg::KeySymbol
src/app/         界面
                 main_window.h        三栏 + 实例标签页
                 example_tab.h        实例 = 示例对象 + 独立参数 + 宿主
                 param_panel.h        由 ParamSet 自动生成控件
                 output_panel.h       日志输出区
src/examples/    移植的示例，文件名 = 类名的 snake_case（如 hello_world_example.cpp）
```

## 新增一个示例

1. 在 `src/examples/<分类>/` 新建 `<类名 snake_case>.cpp`，继承 `vsglab::framework::Example`：
   - `info()` 返回原版目录名、分类与简介（`kind` 按有无窗口选 `Window` / `Console`）
   - `declareParams()` 声明参数并保存句柄
   - `start()`：Window 示例构建场景与 viewer 并赋值 `ctx.viewer`；Console 示例执行原版 main() 全部内容
   - `stop()` 释放持有的 vsg 对象
2. 在 `src/framework/example_registry.cpp` 的清单中加一行 `describe(&examples::makeXxx)`
3. 在 `CMakeLists.txt` 的 `VSGLAB_SOURCES` 中加一行

## 已知限制

- vsg::Win32_Window 析构时会 DestroyWindow 外部传入的句柄；当前实现以"每次运行都新建
  NativeSurface、停止后整体丢弃"规避，频繁创建/关闭的稳定性需运行验证
- 多个实例的帧循环都在 Qt 主线程；后台标签页暂停出帧，同时只渲染当前可见实例
- 多窗口类示例（vsgwindows 等）暂不支持
