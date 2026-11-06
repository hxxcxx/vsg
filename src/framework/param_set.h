/**
 * @file param_set.h
 * @brief 示例参数声明：类型化参数句柄与参数分组
 *
 * 示例在 declareParams() 中声明参数并持有返回的句柄，之后直接读 value()；
 * 宿主据声明自动生成界面控件。本层不依赖 Qt。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <vsg/maths/vec4.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vsglab::framework {

/// 参数修改后的生效方式
enum class Apply {
    Live,     ///< 通知示例 onParamChanged()，当场更新
    Restart,  ///< 宿主重启示例（窗口/设备级设置）
};

class ParamBase {
public:
    enum class Kind {
        Bool,
        Int,
        Double,
        Choice,
        File,
        Color,
        Action,
    };

    virtual ~ParamBase() = default;

    Kind kind() const { return kind_; }
    const std::string& label() const { return label_; }
    Apply apply() const { return apply_; }

    /// 恢复默认值（不触发通知）
    virtual void reset() {}

protected:
    ParamBase(Kind kind, std::string label, Apply apply) : kind_(kind), label_(std::move(label)), apply_(apply) {}

    void notifyChanged() {
        if (listener_)
            listener_(*this);
    }

private:
    friend class ParamSet;

    Kind kind_;
    std::string label_;
    Apply apply_;
    std::function<void(ParamBase&)> listener_;
};

template <typename T>
class Param : public ParamBase {
public:
    const T& value() const { return value_; }
    const T& defaultValue() const { return default_; }

    /// 设置新值；值变化时通知监听者
    void set(const T& v) {
        if (value_ == v)
            return;
        value_ = v;
        notifyChanged();
    }

    void reset() override { value_ = default_; }

protected:
    Param(Kind kind, std::string label, T def, Apply apply)
        : ParamBase(kind, std::move(label), apply), value_(def), default_(std::move(def)) {}

private:
    T value_;
    T default_;
};

class BoolParam : public Param<bool> {
public:
    BoolParam(std::string label, bool def, Apply apply) : Param(Kind::Bool, std::move(label), def, apply) {}
};

class IntParam : public Param<int> {
public:
    IntParam(std::string label, int def, int min, int max, Apply apply)
        : Param(Kind::Int, std::move(label), def, apply), min(min), max(max) {}

    const int min;
    const int max;
};

class DoubleParam : public Param<double> {
public:
    DoubleParam(std::string label, double def, double min, double max, double step, Apply apply)
        : Param(Kind::Double, std::move(label), def, apply), min(min), max(max), step(step) {}

    const double min;
    const double max;
    const double step;
};

/// 从固定选项中选一个（值即选项文本）
class ChoiceParam : public Param<std::string> {
public:
    ChoiceParam(std::string label, std::vector<std::string> options, std::string def, Apply apply)
        : Param(Kind::Choice, std::move(label), std::move(def), apply), options(std::move(options)) {}

    const std::vector<std::string> options;
};

/// 文件路径；相对路径按 vsg::Options::paths 查找。suggestions 作为下拉候选。
class FileParam : public Param<std::string> {
public:
    FileParam(std::string label, std::string def, std::string filter, std::vector<std::string> suggestions, Apply apply)
        : Param(Kind::File, std::move(label), std::move(def), apply),
          filter(std::move(filter)),
          suggestions(std::move(suggestions)) {}

    const std::string filter;  ///< Qt 文件对话框过滤器，如 "VSG Models (*.vsgt *.vsgb)"
    const std::vector<std::string> suggestions;
};

/// RGBA 颜色，分量 0..1
class ColorParam : public Param<vsg::vec4> {
public:
    ColorParam(std::string label, vsg::vec4 def, Apply apply) : Param(Kind::Color, std::move(label), def, apply) {}
};

/// 按钮：没有值，触发即通知（总是 Live）
class ActionParam : public ParamBase {
public:
    explicit ActionParam(std::string label) : ParamBase(Kind::Action, std::move(label), Apply::Live) {}

    void trigger() { notifyChanged(); }
};

/// 一组参数，界面上对应一个分组
class ParamSet {
public:
    explicit ParamSet(std::string title = {}) : title_(std::move(title)) {}

    ParamSet(const ParamSet&) = delete;
    ParamSet& operator=(const ParamSet&) = delete;

    const std::string& title() const { return title_; }

    BoolParam* addBool(std::string label, bool def, Apply apply = Apply::Live);
    IntParam* addInt(std::string label, int def, int min, int max, Apply apply = Apply::Live);
    DoubleParam* addDouble(std::string label, double def, double min, double max, double step = 0.1,
                           Apply apply = Apply::Live);
    ChoiceParam* addChoice(std::string label, std::vector<std::string> options, std::string def,
                           Apply apply = Apply::Live);
    FileParam* addFile(std::string label, std::string def, std::string filter,
                       std::vector<std::string> suggestions = {}, Apply apply = Apply::Restart);
    ColorParam* addColor(std::string label, vsg::vec4 def, Apply apply = Apply::Live);
    ActionParam* addAction(std::string label);

    const std::vector<std::unique_ptr<ParamBase>>& params() const { return params_; }
    bool empty() const { return params_.empty(); }

    /// 任一参数变化时回调（宿主设置）
    void setListener(std::function<void(ParamBase&)> listener);

    /// 全部恢复默认值（不触发通知）
    void resetAll();

private:
    template <typename P, typename... Args>
    P* add(Args&&... args);

    std::string title_;
    std::vector<std::unique_ptr<ParamBase>> params_;
    std::function<void(ParamBase&)> listener_;
};

}  // namespace vsglab::framework
