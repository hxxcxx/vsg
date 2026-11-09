/**
 * @file example_tab.h
 * @brief 一个示例实例对应的标签页：持有示例对象、示例参数与运行器
 *
 * 创建时按示例继承的基类选择运行器：ConsoleExample → ConsoleRunner，WindowExample → WindowRunner；
 * 之后的启动、停止、参数变化都统一交给运行器。
 * 同一示例可以创建多个实例，参数各自独立、从默认值开始。
 *
 * @author hxxcxx
 * @date 2026-11-07
 */
#pragma once

#include "framework/example.h"
#include "framework/param_set.h"

#include <QWidget>

#include <memory>

namespace vsglab::framework {
struct ExampleDescriptor;
class WindowParams;
}  // namespace vsglab::framework

namespace vsglab::host {
class ExampleRunner;
class LogBridge;
class WindowRunner;
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

    /// 示例参数
    vsglab::framework::ParamSet& params() { return params_; }

    /// 窗口参数；控制台示例没有窗口，返回 nullptr
    vsglab::framework::WindowParams* windowParams();

    void start();
    void restart();
    void stop();

signals:
    void started();
    void stopped();
    void finished();
    void failed(const QString& message);

private:
    vsglab::framework::ExampleInfo info_;
    QString tag_;
    std::unique_ptr<vsglab::framework::Example> example_;
    vsglab::framework::ParamSet params_{ "Example" };

    vsglab::host::ExampleRunner* runner_ = nullptr;
    vsglab::host::WindowRunner* window_runner_ = nullptr;  // 仅窗口示例，与 runner_ 指向同一对象
};
