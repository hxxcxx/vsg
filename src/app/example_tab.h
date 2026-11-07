/**
 * @file example_tab.h
 * @brief 一个示例实例对应的标签页：持有示例对象、参数、窗口参数与运行宿主
 *
 * Window 示例显示 VSG 视口；Console 示例显示本实例的输出。
 * 同一示例可以创建多个实例，参数各自独立、从默认值开始。
 *
 * @author hxxcxx
 * @date 2026-11-07
 */
#pragma once

#include "framework/example.h"
#include "framework/param_set.h"
#include "framework/window_params.h"
#include "host/log_bridge.h"

#include <QWidget>

#include <memory>

class OutputPanel;

namespace vsglab::framework {
struct ExampleDescriptor;
}  // namespace vsglab::framework

namespace vsglab::host {
class VsgHost;
}  // namespace vsglab::host

class ExampleTab : public QWidget {
    Q_OBJECT
public:
    /// tag 为实例名（如 "vsghelloworld #2"），用于标签标题与日志标注
    ExampleTab(const vsglab::framework::ExampleDescriptor& descriptor, const QString& tag,
               vsglab::host::LogBridge* logBridge, QWidget* parent = nullptr);
    ~ExampleTab();

    const vsglab::framework::ExampleInfo& info() const { return info_; }
    const QString& tag() const { return tag_; }
    bool isConsole() const { return info_.kind == vsglab::framework::ExampleKind::Console; }

    vsglab::framework::ParamSet& params() { return params_; }
    vsglab::framework::WindowParams& windowParams() { return window_params_; }
    vsglab::host::VsgHost* host() const { return host_; }

    void start();
    void restart();
    void stop();

private slots:
    void onLogMessage(vsglab::host::LogBridge::Level level, const QString& text, const QString& tag);
    void onExampleFailed(const QString& message);

private:
    vsglab::framework::ExampleInfo info_;
    QString tag_;
    std::unique_ptr<vsglab::framework::Example> example_;
    vsglab::framework::ParamSet params_{ "Example" };
    vsglab::framework::WindowParams window_params_;

    vsglab::host::VsgHost* host_ = nullptr;
    OutputPanel* console_ = nullptr;  // 仅 Console 示例
};
