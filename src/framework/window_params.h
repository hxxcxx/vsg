/**
 * @file window_params.h
 * @brief 所有示例共用的窗口参数（对应原版 WindowTraits 的命令行选项）
 *
 * 都是窗口/设备创建期的设置，因此全部为 Apply::Restart。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/param_set.h"

#include <vsg/app/WindowTraits.h>

namespace vsglab::framework {

class WindowParams {
public:
    WindowParams();

    ParamSet& params() { return params_; }

    /// 把当前参数写入 traits
    void applyTo(vsg::WindowTraits& traits) const;

private:
    ParamSet params_{ "Window" };

    BoolParam* debug_layer_ = nullptr;
    BoolParam* api_dump_layer_ = nullptr;
    BoolParam* synchronization_layer_ = nullptr;
    ChoiceParam* image_count_ = nullptr;
    ChoiceParam* present_mode_ = nullptr;
    ChoiceParam* surface_format_ = nullptr;
    ChoiceParam* samples_ = nullptr;
    ChoiceParam* device_preference_ = nullptr;
};

}  // namespace vsglab::framework
