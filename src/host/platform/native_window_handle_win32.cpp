/**
 * @file native_window_handle_win32.cpp
 * @brief Qt/Win32 原生窗口句柄适配
 * @author hxxcxx
 * @date 2026-11-06
 */

#include "host/platform/native_window_handle.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <QWidget>

namespace vsglab::host {

void assignNativeWindowHandle(vsg::WindowTraits& traits, QWidget& widget) {
    // vsg::Win32_Window 只读取 nativeWindow；systemConnection 仅在其自建窗口时作为输出
    traits.nativeWindow = reinterpret_cast<HWND>(widget.winId());
}

}  // namespace vsglab::host
