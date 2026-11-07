/**
 * @file example.h
 * @brief 示例抽象基类，每个 vsgExamples 原版示例对应一个子类
 *
 * 每次在界面上创建实例都会 new 一个新对象，因此示例可以放心用成员保存状态。
 *
 * 生命周期（由宿主驱动）：
 *   Window 示例：declareParams() 只调用一次 → start() → 每帧 frame() → stop()
 *   Console 示例：declareParams() 只调用一次 → start()（一次性跑完）→ stop()
 *   重启 = stop() + start()，参数值保留。
 *
 * 线程：Window 示例的所有回调都在界面线程；Console 示例的 start() 在后台线程执行
 * （不阻塞界面，各 Console 示例排队依次运行），其余回调在界面线程。
 *
 * 输出：原版代码中的 std::cout / vsg::info 等直接保留即可，宿主会把它们标上实例名，
 * Console 示例的输出同时显示在其标签页内。
 *
 * 约定：stop() 中释放示例持有的所有 vsg 对象（尤其是 Window / Camera 等），
 * 以便宿主在 stop() 之后销毁窗口。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/example_context.h"
#include "framework/param_set.h"

#include <string>

namespace vsglab::framework {

/// 示例形态
enum class ExampleKind {
    Window,   ///< 创建 viewer 与窗口，宿主驱动帧循环
    Console,  ///< 不创建窗口，start() 中一次性运行完毕（如 core/maths 系列）
};

struct ExampleInfo {
    std::string id;        ///< 原版目录名，如 "vsghelloworld"
    std::string category;  ///< 原版分类，如 "app"
    std::string brief;     ///< 一句话说明
    ExampleKind kind = ExampleKind::Window;
};

class Example {
public:
    virtual ~Example() = default;

    virtual ExampleInfo info() const = 0;

    /// 声明参数并保存返回的句柄
    virtual void declareParams(ParamSet& /*params*/) {}

    /// Window 示例：构建场景与 viewer，并赋值给 ctx.viewer（对应原版 main() 中进入循环之前的部分）；
    /// Console 示例：执行原版 main() 的全部内容。
    virtual void start(ExampleContext& ctx) = 0;

    /// 渲染一帧（仅 Window 示例）。宿主已调用 advanceToNextFrame() 并注入了界面事件。
    virtual void frame(ExampleContext& ctx) {
        ctx.viewer->handleEvents();
        ctx.viewer->update();
        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

    /// Live 参数变化或 Action 触发时调用（仅在 Window 示例运行期间）
    virtual void onParamChanged(ExampleContext& /*ctx*/, const ParamBase& /*param*/) {}

    /// 释放示例持有的 vsg 对象
    virtual void stop(ExampleContext& /*ctx*/) {}
};

}  // namespace vsglab::framework
