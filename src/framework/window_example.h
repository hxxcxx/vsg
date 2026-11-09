/**
 * @file window_example.h
 * @brief 窗口形态的示例：创建 viewer 与窗口，由宿主驱动帧循环
 *
 * 对应原版中打开窗口渲染的示例，原版 main() 拆成三段：
 *   start()：进入渲染循环之前的部分，建场景、建 viewer、compile，并把 viewer 交给 ctx.viewer
 *   frame()：渲染循环的一次循环体（默认实现与原版相同）
 *   stop()：释放示例持有的 vsg 对象（尤其是 Window / Camera 等），宿主随后销毁窗口
 * 所有回调都在界面线程调用。修改 Restart 类参数会 stop() + start()，Live 类参数调用 onParamChanged()。
 *
 * @author hxxcxx
 * @date 2026-11-09
 */
#pragma once

#include "framework/example.h"

#include <vsg/app/Viewer.h>
#include <vsg/app/WindowTraits.h>
#include <vsg/io/Options.h>

#include <functional>

namespace vsglab::framework {

/// 窗口示例运行时可用的宿主服务
class WindowContext {
public:
    using WindowTraitsFactory = std::function<vsg::ref_ptr<vsg::WindowTraits>()>;

    WindowContext(vsg::ref_ptr<vsg::Options> options, WindowTraitsFactory makeWindowTraits)
        : options_(std::move(options)), make_window_traits_(std::move(makeWindowTraits)) {}

    /// 已按公共窗口参数设置好、并填入嵌入用原生窗口句柄与物理像素尺寸的 traits。
    /// 每次调用返回新对象，示例可再按需修改。
    vsg::ref_ptr<vsg::WindowTraits> windowTraits() const { return make_window_traits_(); }

    /// 文件读取选项，同 ConsoleContext::options()
    vsg::ref_ptr<vsg::Options> options() const { return options_; }

    /// 示例在 start() 中创建并赋值；宿主据此驱动帧循环与销毁。
    vsg::ref_ptr<vsg::Viewer> viewer;

private:
    vsg::ref_ptr<vsg::Options> options_;
    WindowTraitsFactory make_window_traits_;
};

class WindowExample : public Example {
public:
    /// 构建场景与 viewer，并赋值给 ctx.viewer
    virtual void start(WindowContext& ctx) = 0;

    /// 渲染一帧。宿主已调用 advanceToNextFrame() 并注入了界面事件。
    virtual void frame(WindowContext& ctx) {
        ctx.viewer->handleEvents();
        ctx.viewer->update();
        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

    /// Live 参数变化或 Action 触发时调用（仅在运行期间）
    virtual void onParamChanged(WindowContext& /*ctx*/, const ParamBase& /*param*/) {}

    /// 释放示例持有的 vsg 对象
    virtual void stop(WindowContext& /*ctx*/) {}
};

}  // namespace vsglab::framework
