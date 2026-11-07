/**
 * @file vsg_host.h
 * @brief 示例运行宿主：一个示例实例的生命周期、帧循环与异常隔离
 *
 * Window 示例：
 *   启动：新建 NativeSurface → example->start(ctx) → 帧定时器
 *   每帧：advanceToNextFrame() → 注入界面事件 → example->frame(ctx)
 *   停止：example->stop() → deviceWaitIdle() → 释放 viewer/window → 丢弃 NativeSurface
 *   控件隐藏（如标签页切走）时暂停帧定时器，重新显示时恢复。
 * Console 示例：
 *   启动即在 start() 中一次性运行完毕，随后自动 stop() 并发出 finished。
 *
 * 示例代码的调用都处在 LogTagScope 内，日志标注为本实例。
 * 示例抛出的异常在此捕获并通过 errorOccurred 报告，不影响程序运行。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/example.h"
#include "framework/example_context.h"

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <string>

class QLabel;
class QVBoxLayout;

namespace vsglab::framework {
class WindowParams;
}  // namespace vsglab::framework

namespace vsglab::host {

class NativeSurface;

class VsgHost : public QWidget {
    Q_OBJECT
public:
    explicit VsgHost(const framework::WindowParams& windowParams, QWidget* parent = nullptr);
    ~VsgHost();

    /// 日志中标注的实例名
    void setLogTag(std::string tag) { log_tag_ = std::move(tag); }

    /// 停止当前示例并运行新示例（example 由调用方持有；传 nullptr 仅停止并解除引用）
    void run(framework::Example* example);

    /// 以当前参数重启当前示例
    void restart();

    /// 停止当前示例（示例对象保留）
    void stop();

    framework::Example* current() const { return example_; }
    bool isRunning() const { return running_; }

    /// 参数变化：Restart 类重启示例；Live/Action 转给运行中的示例
    void handleParamChanged(framework::ParamBase& param);

signals:
    void started();
    void stopped();
    void finished();  ///< Console 示例运行完毕
    void errorOccurred(const QString& message);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void onFrameTick();
    void onSurfaceResized();

private:
    bool isConsole() const;
    void tryStartPending();
    void startNow();
    void startWindow();
    void startConsole();
    void teardown();
    void fail(const QString& what);
    void showPlaceholder(const QString& text);

    const framework::WindowParams& window_params_;
    vsg::ref_ptr<vsg::Options> options_;
    std::string log_tag_;

    QVBoxLayout* layout_ = nullptr;
    QLabel* placeholder_ = nullptr;
    QPointer<NativeSurface> surface_;
    QTimer timer_;

    framework::Example* example_ = nullptr;
    std::unique_ptr<framework::ExampleContext> ctx_;
    bool pending_start_ = false;
    bool restart_queued_ = false;
    bool running_ = false;
};

}  // namespace vsglab::host
