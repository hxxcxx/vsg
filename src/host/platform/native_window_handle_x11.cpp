/**
 * @file native_window_handle_x11.cpp
 * @brief Qt/X11 原生窗口句柄适配
 *
 * 当前 VSG 没有 Wayland 后端，main() 在未指定平台时强制 QT_QPA_PLATFORM=xcb。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */

#include "host/platform/native_window_handle.h"

#include <QGuiApplication>
#include <QWidget>
#include <QtGui/qguiapplication_platform.h>

#include <cstdint>

namespace vsglab::host {

void assignNativeWindowHandle(vsg::WindowTraits& traits, QWidget& widget) {
    // vsg::Xcb_Window 以 xcb_window_t(uint32_t) 接收窗口、以 xcb_connection_t* 接收连接；
    // 复用 Qt 的连接，保证窗口与 VSG 处于同一 X 连接。
    traits.nativeWindow = static_cast<uint32_t>(widget.winId());
    if (auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
        traits.systemConnection = x11->connection();
}

}  // namespace vsglab::host
