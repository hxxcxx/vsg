/**
 * @file visitor_custom_type_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgvisitorcustomtype
 *
 * 让 Visitor 支持自定义节点类型的两种方式：
 * 1. 在 Visitor 基类的 apply(Group&) 中识别自定义类型（visitor_custom_type.h）
 * 2. 自定义节点重写 accept() 主动分派（alternate_visitor_custom_type.h）
 * 参考 https://groups.google.com/g/vsg-users/c/YpSZwrKlcKI
 *     https://groups.google.com/g/vsg-users/c/dpazkzRLO_8/m/TsqQWnN6AQAJ
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "examples/core/alternate_visitor_custom_type.h"
#include "examples/core/visitor_custom_type.h"
#include "framework/example.h"

#include <iostream>

namespace vsglab::examples {
namespace {

using framework::ExampleContext;
using framework::ExampleInfo;
using framework::ExampleKind;
using namespace visitor_custom_type;

class VisitorCustomTypeExample : public framework::Example {
public:
    ExampleInfo info() const override {
        return { "vsgvisitorcustomtype", "core",
                 "Two ways to let visitors handle custom node types: recognise them in a visitor base class's "
                 "apply(Group&), or override accept() in the custom node to dispatch itself.",
                 ExampleKind::Console };
    }

    void start(ExampleContext& /*ctx*/) override {
        // Approach 1
        {
            std::cout << "First approach to implementing custom types and custom visitors that support these custom "
                         "types."
                      << std::endl;

            auto group = vsg::Group::create();

            auto child1 = CustomGroupNode::create();
            auto child2 = CustomLODNode::create();

            group->addChild(child1);
            group->addChild(child2);

            VisitCustomTypes v;
            group->accept(v);
        }

        // Approach 2
        {
            std::cout << "\nSecond/Alternate approach to implementing custom types and custom visitors that support "
                         "these custom types."
                      << std::endl;

            auto group = vsg::Group::create();

            auto child1 = AlternateCustomGroupNode::create();
            auto child2 = AlternateCustomLODNode::create();

            group->addChild(child1);
            group->addChild(child2);

            AlternateVisitCustomTypes v;
            group->accept(v);
        }
    }
};

}  // namespace

std::unique_ptr<framework::Example> makeVisitorCustomTypeExample() {
    return std::make_unique<VisitorCustomTypeExample>();
}

}  // namespace vsglab::examples
