#include "host/log_bridge.h"

#include <QPointer>

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>

#include <cstdio>

namespace vsglab::host {
namespace {

/// 转发到 LogBridge 的 vsg::Logger；同时写一份到控制台（C stdio 不受 redirect_std 影响）
class QtLogger : public vsg::Inherit<vsg::Logger, QtLogger> {
public:
    explicit QtLogger(LogBridge* bridge) : bridge_(bridge) {}

protected:
    void debug_implementation(const std::string_view& message) override {
        forward(LogBridge::Level::Debug, message, stdout);
    }
    void info_implementation(const std::string_view& message) override {
        forward(LogBridge::Level::Info, message, stdout);
    }
    void warn_implementation(const std::string_view& message) override {
        forward(LogBridge::Level::Warn, message, stderr);
    }
    void error_implementation(const std::string_view& message) override {
        forward(LogBridge::Level::Error, message, stderr);
    }

    void fatal_implementation(const std::string_view& message) override {
        forward(LogBridge::Level::Fatal, message, stderr);
        // 与 vsg::StdLogger 一致：fatal 抛异常，由宿主捕获
        throw vsg::Exception{ std::string(message) };
    }

private:
    void forward(LogBridge::Level level, std::string_view message, FILE* console) {
        // redirect_std 转来的输出通常带行尾换行
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
            message.remove_suffix(1);
        if (message.empty())
            return;

        std::fprintf(console, "%.*s\n", static_cast<int>(message.size()), message.data());

        if (bridge_)
            emit bridge_->message(level, QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size())));
    }

    QPointer<LogBridge> bridge_;
};

}  // namespace

LogBridge::LogBridge(QObject* parent) : QObject(parent) {
    auto logger = QtLogger::create(this);
    logger->level = vsg::Logger::LOGGER_INFO;
    logger->redirect_std();
    vsg::Logger::instance() = logger;
}

LogBridge::~LogBridge() {
    // 替换后旧 logger 析构，std::cout / std::cerr 随之恢复
    vsg::Logger::instance() = vsg::StdLogger::create();
}

}  // namespace vsglab::host
