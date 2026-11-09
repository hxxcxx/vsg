/**
 * @file console_runner.h
 * @brief 控制台示例的运行器：在后台线程跑一次 run()，并显示本实例的输出
 *
 * 所有控制台示例共用一个后台线程、排队依次执行：VSG 把 std::cout 重定向到 Logger 时
 * 使用共享的行缓冲，多个线程同时写会把字符交错在一起。
 * 运行中停止/重新运行/销毁时会等待其结束；仍在排队、尚未开始的直接取消。
 *
 * @author hxxcxx
 * @date 2026-11-09
 */
#pragma once

#include "framework/console_example.h"
#include "host/example_runner.h"
#include "host/log_bridge.h"

#include <memory>

namespace vsglab::host {

class OutputPanel;

class ConsoleRunner : public ExampleRunner {
    Q_OBJECT
public:
    ConsoleRunner(framework::ConsoleExample& example, std::string logTag, LogBridge* logBridge,
                  QWidget* parent = nullptr);
    ~ConsoleRunner();

    void start() override;
    void stop() override;
    void restart() override;

private slots:
    void onLogMessage(vsglab::host::LogBridge::Level level, const QString& text, const QString& tag);

private:
    struct Run;

    void onRunFinished(const std::shared_ptr<Run>& run, bool failed, const QString& error);
    void waitForRun();

    framework::ConsoleExample& example_;
    QString tag_;
    OutputPanel* output_ = nullptr;

    std::unique_ptr<framework::ConsoleContext> ctx_;
    std::shared_ptr<Run> run_;  // 当前在后台线程上的运行（排队中或运行中）
};

}  // namespace vsglab::host
