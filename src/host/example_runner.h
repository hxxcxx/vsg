/**
 * @file example_runner.h
 * @brief 示例运行器基类：驱动一个示例实例的生命周期，并作为它在标签页中的视图
 *
 * 两种形态各有一个子类：
 *   WindowRunner（window_runner.h）：嵌入原生窗口，界面线程上驱动帧循环
 *   ConsoleRunner（console_runner.h）：后台线程跑一次，显示本实例的输出
 * 本类负责两者相同的部分：参数变化的分派、生命周期信号、读取选项与日志标签。
 *
 * @author hxxcxx
 * @date 2026-11-09
 */
#pragma once

#include "framework/param_set.h"

#include <QWidget>

#include <vsg/io/Options.h>

#include <string>

namespace vsglab::host {

class ExampleRunner : public QWidget {
    Q_OBJECT
public:
    /// logTag 为实例名（如 "vsghelloworld #2"），示例代码产生的日志都标注为它
    explicit ExampleRunner(std::string logTag, QWidget* parent = nullptr);

    /// 运行示例
    virtual void start() = 0;

    /// 停止示例；运行中的控制台示例会等待其结束
    virtual void stop() = 0;

    /// 以当前参数重新运行
    virtual void restart();

    /// 参数变化：Restart 类重新运行（同一轮事件中的多次修改合并为一次）；Live/Action 交给子类
    void handleParamChanged(framework::ParamBase& param);

signals:
    void started();
    void stopped();
    void finished();  ///< 控制台示例运行完毕
    void errorOccurred(const QString& message);

protected:
    /// Live 参数变化或 Action 触发
    virtual void onLiveParamChanged(framework::ParamBase& /*param*/) {}

    const std::string& logTag() const { return log_tag_; }
    const vsg::ref_ptr<vsg::Options>& options() const { return options_; }

private:
    std::string log_tag_;
    vsg::ref_ptr<vsg::Options> options_;
    bool restart_queued_ = false;
};

}  // namespace vsglab::host
