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

- 左侧清单按 vsgExamples 的顺序排列；**双击**示例（或选中后点工具栏 New Instance）创建一个实例标签页，可重复创建
- 每个实例的参数独立、从默认值开始；窗口参数组为 Restart 类（修改后自动重启示例）
- 控制台示例（如 core/maths，无窗口）运行一次，结果显示在其标签页内；它们在后台线程执行、
  不阻塞界面，多个控制台示例排队依次运行；运行中关闭或重启标签会等待其结束
- 底部输出区显示 vsg 日志与 `std::cout`，带实例标签（如 `[vsghelloworld #2]`）
- 修改 Restart 类参数（含控制台示例的参数）会自动重新运行该实例

## 代码结构

```
src/framework/   示例接口与参数系统（不依赖 Qt）
                 example.h            Example 公共基类：描述信息 + 参数声明
                 console_example.h    ConsoleExample：run() 跑一次（后台线程）
                 window_example.h     WindowExample：start() / frame() / stop()（界面线程）
                 example_registry.h   VSGLAB_REGISTER_EXAMPLE 登记宏；按课程表（vsgExamples 顺序）排列
                 param_set.h          类型化参数句柄（Live / Restart）
                 window_params.h      公共窗口参数（对应 WindowTraits 命令行选项）
src/host/        运行示例（Qt 层，但不包含界面布局）
                 example_runner.h     运行器基类：参数分派、生命周期信号、读取选项
                 window_runner.h      窗口示例：嵌入原生窗口、帧循环、隐藏时暂停出帧
                 console_runner.h     控制台示例：后台线程跑一次、显示本实例的输出
                 native_surface.h     原生子窗口 + Qt→vsg 事件桥（键鼠/HiDPI）
                 platform/            各平台原生句柄写入（_win32 / _x11 / _macos）
                 log_bridge.h         vsg::Logger → 界面，thread_local 实例标签
                 output_panel.h       日志输出区（主窗口底部与控制台示例共用）
                 key_map.h            Qt 键码 → vsg::KeySymbol
src/app/         界面
                 main_window.h        三栏 + 实例标签页
                 example_tab.h        实例 = 示例对象 + 示例参数 + 运行器（按示例形态选择）
                 param_panel.h        由 ParamSet 自动生成控件
src/examples/    移植的示例，按原版分类建子目录，文件名 = 类名的 snake_case（如 core/arrays_example.cpp）；
                 需要额外头文件的示例整体放进同名子目录（如 core/visitor_custom_type/）；
                 示例自定义的类型放在匿名命名空间或示例专用命名空间中，避免与其他示例同名冲突
```

## 新增一个示例

1. 在 `src/examples/<分类>/` 新建 `<类名 snake_case>.cpp`，按形态继承其一：
   - **控制台示例**继承 `framework::ConsoleExample`，在 `run()` 中放原版 `main()` 的全部内容
   - **窗口示例**继承 `framework::WindowExample`：`start()` 建场景与 viewer 并赋值 `ctx.viewer`，
     `stop()` 释放持有的 vsg 对象；`frame()` 默认即原版渲染循环的循环体
   - 有参数时重写 `declareParams()`，保存返回的参数句柄，运行时读 `->value()`
2. 在同一文件末尾登记：
   ```cpp
   VSGLAB_REGISTER_EXAMPLE(ArraysExample, "vsgarrays", "core", "一句话说明");
   ```
3. 在 `CMakeLists.txt` 的示例列表中加一行

左侧清单的位置由课程表（`example_registry.cpp`）决定，与登记先后无关。

## 已知限制

- vsg::Win32_Window 析构时会 DestroyWindow 外部传入的句柄（Qt 会打印一条 `External WM_DESTROY received`
  警告）；当前实现以"每次运行都新建 NativeSurface、停止后整体丢弃"规避
- 多个实例的帧循环都在 Qt 主线程；后台标签页暂停出帧，同时只渲染当前可见实例
- 同时运行的窗口示例实例数受 VSG 编译选项 `VSG_MAX_DEVICES` 限制（当前为 4）
- 多窗口类示例（vsgwindows 等）暂不支持
