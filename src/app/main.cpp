#include "app/main_window.h"

#include <QApplication>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

int main(int argc, char* argv[]) {
#if defined(__linux__)
    // 当前 VSG 没有 Wayland 后端，窗口嵌入基于 X11/XCB；仅在用户未显式指定平台时选择 XCB。
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
#endif

#ifdef _WIN32
    // 控制台按 UTF-8 输出日志
    SetConsoleOutputCP(CP_UTF8);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("vsglab"));

    MainWindow window;
    window.show();

    return QApplication::exec();
}
