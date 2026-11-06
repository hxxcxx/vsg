#include "framework/param_set.h"

namespace vsglab::framework {

template <typename P, typename... Args>
P* ParamSet::add(Args&&... args) {
    auto param = std::make_unique<P>(std::forward<Args>(args)...);
    P* raw = param.get();
    raw->listener_ = listener_;
    params_.push_back(std::move(param));
    return raw;
}

BoolParam* ParamSet::addBool(std::string label, bool def, Apply apply) {
    return add<BoolParam>(std::move(label), def, apply);
}

IntParam* ParamSet::addInt(std::string label, int def, int min, int max, Apply apply) {
    return add<IntParam>(std::move(label), def, min, max, apply);
}

DoubleParam* ParamSet::addDouble(std::string label, double def, double min, double max, double step, Apply apply) {
    return add<DoubleParam>(std::move(label), def, min, max, step, apply);
}

ChoiceParam* ParamSet::addChoice(std::string label, std::vector<std::string> options, std::string def, Apply apply) {
    return add<ChoiceParam>(std::move(label), std::move(options), std::move(def), apply);
}

FileParam* ParamSet::addFile(std::string label, std::string def, std::string filter,
                             std::vector<std::string> suggestions, Apply apply) {
    return add<FileParam>(std::move(label), std::move(def), std::move(filter), std::move(suggestions), apply);
}

ColorParam* ParamSet::addColor(std::string label, vsg::vec4 def, Apply apply) {
    return add<ColorParam>(std::move(label), def, apply);
}

ActionParam* ParamSet::addAction(std::string label) {
    return add<ActionParam>(std::move(label));
}

void ParamSet::setListener(std::function<void(ParamBase&)> listener) {
    listener_ = std::move(listener);
    for (auto& param : params_)
        param->listener_ = listener_;
}

void ParamSet::resetAll() {
    for (auto& param : params_)
        param->reset();
}

}  // namespace vsglab::framework
