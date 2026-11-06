/**
 * @file vsg_host.h
 * @brief 示例运行宿主：示例生命周期、帧循环与异常隔离
 *
 * 启动：新建 NativeSurface → example->start(ctx) → 帧定时器
 * 每帧：advanceToNextFrame() → 注入界面事件 → example->frame(ctx)
 * 停止：example->stop() → deviceWaitIdle() → 释放 viewer/window → 丢弃 NativeSurface
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
    void started(const QString& id);
    void stopped();
    void errorOccurred(const QString& message);

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onFrameTick();
    void onSurfaceResized();

private:
    void tryStartPending();
    void startNow();
    void teardown();
    void fail(const QString& what);
    void showPlaceholder(const QString& text);

    const framework::WindowParams& window_params_;
    vsg::ref_ptr<vsg::Options> options_;

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
