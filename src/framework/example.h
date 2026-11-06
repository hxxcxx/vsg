/**
 * @file example.h
 * @brief 示例抽象基类，每个 vsgExamples 原版示例对应一个子类
 *
 * 生命周期（由宿主驱动）：
 *   declareParams() 只调用一次 → start() → 每帧 frame() → stop()
 *   重启 = stop() + start()，参数值保留。
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

struct ExampleInfo {
    std::string id;        ///< 原版目录名，如 "vsghelloworld"
    std::string category;  ///< 原版分类，如 "app"
    std::string brief;     ///< 一句话说明
};

class Example {
public:
    virtual ~Example() = default;

    virtual ExampleInfo info() const = 0;

    /// 声明参数并保存返回的句柄
    virtual void declareParams(ParamSet& /*params*/) {}

    /// 构建场景与 viewer，并赋值给 ctx.viewer（对应原版 main() 中进入循环之前的部分）
    virtual void start(ExampleContext& ctx) = 0;

    /// 渲染一帧。宿主已调用 advanceToNextFrame() 并注入了界面事件。
    virtual void frame(ExampleContext& ctx) {
        ctx.viewer->handleEvents();
        ctx.viewer->update();
        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

    /// Live 参数变化或 Action 触发时调用（仅在示例运行期间）
    virtual void onParamChanged(ExampleContext& /*ctx*/, const ParamBase& /*param*/) {}

    /// 释放示例持有的 vsg 对象
    virtual void stop(ExampleContext& /*ctx*/) {}
};

}  // namespace vsglab::framework
