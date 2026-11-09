/**
 * @file example.h
 * @brief 示例的公共基类与描述信息
 *
 * 每个 vsgExamples 原版示例移植为一个子类，按形态继承两者之一：
 *   ConsoleExample（console_example.h）：不创建窗口，跑一次打印结果，如 core / maths 系列
 *   WindowExample（window_example.h）：创建窗口并持续渲染，如 vsghelloworld
 * 写好后在同一个 .cpp 末尾用 VSGLAB_REGISTER_EXAMPLE 登记（example_registry.h），不需要改动其他文件。
 *
 * 每次在界面上创建实例都会 new 一个新对象，因此示例可以放心用成员保存状态。
 * 原版代码中的 std::cout / vsg::info 等直接保留即可，宿主会把输出标上实例名。
 *
 * 本层不依赖 Qt。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/param_set.h"

#include <string>

namespace vsglab::framework {

/// 示例的描述信息，由 VSGLAB_REGISTER_EXAMPLE 登记
struct ExampleInfo {
    std::string id;        ///< 原版目录名，如 "vsghelloworld"
    std::string category;  ///< 原版分类，如 "app"
    std::string brief;     ///< 一句话说明
};

class Example {
public:
    virtual ~Example() = default;

    /// 声明参数并保存返回的句柄；创建实例后调用一次，重启时参数值保留
    virtual void declareParams(ParamSet& /*params*/) {}
};

}  // namespace vsglab::framework
