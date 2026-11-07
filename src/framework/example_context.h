/**
 * @file example_context.h
 * @brief 示例运行时可用的宿主服务（窗口参数、文件读取选项、viewer 交接）
 *
 * 本层不依赖 Qt。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <vsg/app/Viewer.h>
#include <vsg/app/WindowTraits.h>
#include <vsg/core/Exception.h>
#include <vsg/io/Options.h>

#include <functional>

namespace vsglab::host {
class VsgHost;
}  // namespace vsglab::host

namespace vsglab::framework {

class ExampleContext {
public:
    /// 已按公共窗口参数设置好、并填入嵌入用原生窗口句柄与物理像素尺寸的 traits。
    /// 每次调用返回新对象，示例可再按需修改。Console 示例没有窗口，调用会抛异常。
    vsg::ref_ptr<vsg::WindowTraits> windowTraits() const {
        if (!make_window_traits_)
            throw vsg::Exception{ "windowTraits() is not available in a Console example" };
        return make_window_traits_();
    }

    /// 文件读取选项：paths 已包含 vsgExamples/data 与 VSG_FILE_PATH。
    vsg::ref_ptr<vsg::Options> options() const { return options_; }

    /// 示例在 start() 中创建并赋值；宿主据此驱动帧循环与销毁。
    vsg::ref_ptr<vsg::Viewer> viewer;

private:
    friend class host::VsgHost;

    std::function<vsg::ref_ptr<vsg::WindowTraits>()> make_window_traits_;
    vsg::ref_ptr<vsg::Options> options_;
};

}  // namespace vsglab::framework
