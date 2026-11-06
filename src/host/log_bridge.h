/**
 * @file log_bridge.h
 * @brief 把 vsg::Logger（含被重定向的 std::cout / std::cerr）转发到 Qt 界面
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <QObject>
#include <QString>

namespace vsglab::host {

class LogBridge : public QObject {
    Q_OBJECT
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
    };
    Q_ENUM(Level)

    /// 安装为 vsg::Logger::instance()，并重定向 std::cout / std::cerr
    explicit LogBridge(QObject* parent = nullptr);

    /// 恢复 vsg 默认 Logger
    ~LogBridge();

signals:
    /// 可能在任意线程发出；连接时使用 Qt::QueuedConnection
    void message(vsglab::host::LogBridge::Level level, const QString& text);
};

}  // namespace vsglab::host
