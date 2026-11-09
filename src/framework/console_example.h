/**
 * @file console_example.h
 * @brief 控制台形态的示例：不创建窗口，run() 一次性执行完毕
 *
 * 对应原版中只打印结果的示例（core / maths 等）：run() 里放原版 main() 的全部内容。
 * run() 在后台线程执行，不阻塞界面；所有控制台示例共用一个后台线程，排队依次运行。
 * 输出（std::cout 等）显示在该实例的标签页中。修改参数会重新运行。
 *
 * @author hxxcxx
 * @date 2026-11-09
 */
#pragma once

#include "framework/example.h"

#include <vsg/io/Options.h>

namespace vsglab::framework {

/// 控制台示例运行时可用的宿主服务
class ConsoleContext {
public:
    explicit ConsoleContext(vsg::ref_ptr<vsg::Options> options) : options_(std::move(options)) {}

    /// 文件读取选项：paths 包含 vsgExamples/data 与 VSG_FILE_PATH；编译时找到 vsgXchange 则已挂上其读写器
    vsg::ref_ptr<vsg::Options> options() const { return options_; }

private:
    vsg::ref_ptr<vsg::Options> options_;
};

class ConsoleExample : public Example {
public:
    /// 执行原版 main() 的全部内容（在后台线程调用）
    virtual void run(ConsoleContext& ctx) = 0;
};

}  // namespace vsglab::framework
