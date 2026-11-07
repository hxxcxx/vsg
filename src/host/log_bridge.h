/**
 * @file log_bridge.h
 * @brief 把 vsg::Logger（含被重定向的 std::cout / std::cerr）转发到 Qt 界面，并标注来源实例
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <QObject>
#include <QString>

#include <string>

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
    /// 可能在任意线程发出；连接时使用 Qt::QueuedConnection。
    /// tag 为产生该输出的示例实例名（如 "vsghelloworld #2"），无法归属时为空。
    void message(vsglab::host::LogBridge::Level level, const QString& text, const QString& tag);
};

/**
 * @brief 作用域内当前线程的日志都标注为指定实例
 *
 * 宿主在调用示例代码（start/frame/onParamChanged/stop）时设置。
 * 标签按线程保存，VSG 内部工作线程产生的日志不带标签。
 */
class LogTagScope {
public:
    explicit LogTagScope(std::string tag);
    ~LogTagScope();

    LogTagScope(const LogTagScope&) = delete;
    LogTagScope& operator=(const LogTagScope&) = delete;

private:
    std::string previous_;
};

}  // namespace vsglab::host
