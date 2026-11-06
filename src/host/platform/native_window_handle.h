/**
 * @file native_window_handle.h
 * @brief 把 Qt 控件的原生窗口信息写入 vsg::WindowTraits，供 VSG 嵌入渲染
 *
 * 实现按平台拆分：native_window_handle_win32.cpp / _x11.cpp / _macos.cpp，由 CMake 选择。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <vsg/app/WindowTraits.h>

class QWidget;

namespace vsglab::host {

/**
 * @brief 填写 traits.nativeWindow（及平台需要的 traits.systemConnection）
 *
 * 写入值的类型须与各平台 vsg::Window 实现中的 std::any_cast 完全一致：
 * Win32 为 HWND；X11 为 xcb_window_t 与 xcb_connection_t*；macOS 为 unsigned long long(NSView*)。
 * 句柄归 Qt 所有，但 vsg::Win32_Window 析构时会 DestroyWindow 它，因此宿主每次运行示例都新建控件。
 */
void assignNativeWindowHandle(vsg::WindowTraits& traits, QWidget& widget);

}  // namespace vsglab::host
