/**
 * @file native_window_handle_macos.cpp
 * @brief Qt/macOS 原生窗口句柄适配
 * @author hxxcxx
 * @date 2026-11-06
 */

#include "host/platform/native_window_handle.h"

#include <QWidget>

namespace vsglab::host {

void assignNativeWindowHandle(vsg::WindowTraits& traits, QWidget& widget) {
    // vsg::MacOS_Window 以 unsigned long long 接收 NSView*，并嵌入该视图
    traits.nativeWindow = static_cast<unsigned long long>(widget.winId());
}

}  // namespace vsglab::host
