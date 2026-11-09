/**
 * @file window_runner.h
 * @brief 窗口示例的运行器：嵌入原生窗口交给 VSG 渲染，在界面线程上驱动帧循环
 *
 * 启动：新建 NativeSurface → example.start(ctx) → 帧定时器
 * 每帧：advanceToNextFrame() → 注入界面事件 → example.frame(ctx)
 * 停止：example.stop(ctx) → deviceWaitIdle() → 释放 viewer/window → 丢弃 NativeSurface
 * 控件隐藏（如标签页切走）时暂停帧定时器，重新显示时恢复。
 * 窗口参数（MSAA、呈现模式等）属于本运行器，修改后重启示例。
 *
 * @author hxxcxx
 * @date 2026-11-09
 */
#pragma once

#include "framework/window_example.h"
#include "framework/window_params.h"
#include "host/example_runner.h"

#include <QPointer>
#include <QTimer>

#include <memory>

class QLabel;
class QVBoxLayout;

namespace vsglab::host {

class NativeSurface;

class WindowRunner : public ExampleRunner {
    Q_OBJECT
public:
    WindowRunner(framework::WindowExample& example, std::string logTag, QWidget* parent = nullptr);
    ~WindowRunner();

    void start() override;
    void stop() override;

    framework::WindowParams& windowParams() { return window_params_; }

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void onLiveParamChanged(framework::ParamBase& param) override;

private slots:
    void onFrameTick();
    void onSurfaceResized();

private:
    void tryStartPending();
    void startNow();
    void teardown();
    void fail(const QString& what);
    void showPlaceholder(const QString& text);

    framework::WindowExample& example_;
    framework::WindowParams window_params_;

    QVBoxLayout* layout_ = nullptr;
    QLabel* placeholder_ = nullptr;
    QPointer<NativeSurface> surface_;
    QTimer timer_;

    std::unique_ptr<framework::WindowContext> ctx_;
    bool pending_start_ = false;
    bool running_ = false;
};

}  // namespace vsglab::host
