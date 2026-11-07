/**
 * @file visitor_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgvisitor
 *
 * 构建固定层数的四叉树场景，计时后用自定义 ConstVisitor 统计各类节点数量；
 * 分别演示显式调用 accept() 与 vsg::visit<>() 便捷模板两种用法。
 * 默认 11 层约 560 万个节点，运行需要数秒。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/example.h"

#include <vsg/core/Auxiliary.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/core/visit.h>
#include <vsg/nodes/Group.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::ExampleContext;
using framework::ExampleInfo;
using framework::ExampleKind;
using framework::IntParam;
using framework::ParamSet;

vsg::ref_ptr<vsg::Node> createQuadTree(unsigned int numLevels) {
    if (numLevels == 0)
        return vsg::Node::create();

    vsg::ref_ptr<vsg::Group> t = vsg::Group::create();

    --numLevels;

    t->children.reserve(4);

    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));

    return t;
}

template <typename F>
double time(F function) {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    // do test code
    function();

    return std::chrono::duration<double>(clock::now() - start).count();
}

class VisitorExample : public framework::Example {
public:
    ExampleInfo info() const override {
        return { "vsgvisitor", "core",
                 "Build a fixed quad tree, then count node types with a custom ConstVisitor, both by calling "
                 "accept() explicitly and via the vsg::visit<>() convenience template. "
                 "The default 11 levels create about 5.6 million nodes and take a few seconds.",
                 ExampleKind::Console };
    }

    void declareParams(ParamSet& params) override {
        num_levels_ = params.addInt("Levels (-l)", 11, 0, 12, Apply::Restart);
    }

    void start(ExampleContext& /*ctx*/) override {
        auto numLevels = static_cast<unsigned int>(num_levels_->value());

        vsg::ref_ptr<vsg::Node> scene;
        std::cout << "VulkanSceneGraph Fixed Quad Tree creation : "
                  << time([&]() { scene = createQuadTree(numLevels); }) << std::endl;

        struct MyVisitor : public vsg::ConstVisitor {
            std::map<const char*, uint32_t> objectCounts;

            void apply(const vsg::Object& object) override {
                ++objectCounts[object.className()];

                object.traverse(*this);
            }
        };

        // explicitly create visitor and call accept on the scene
        MyVisitor myVisitor;
        scene->accept(myVisitor);
        std::cout << "MyVisitor() object types s=" << myVisitor.objectCounts.size() << std::endl;
        for (const auto [className, count] : myVisitor.objectCounts) {
            std::cout << "    " << className << " : " << count << std::endl;
        }

        // same functionality but using the vsg::visit<> template convenience function to construct the visitor, call
        // accept and then return the visitor.
        auto objectCounts = vsg::visit<MyVisitor>(scene).objectCounts;
        std::cout << "\nUsing vsg::visit<>() object types s=" << myVisitor.objectCounts.size() << std::endl;
        for (const auto [className, count] : objectCounts) {
            std::cout << "    " << className << " : " << count << std::endl;
        }
    }

private:
    IntParam* num_levels_ = nullptr;
};

}  // namespace

std::unique_ptr<framework::Example> makeVisitorExample() {
    return std::make_unique<VisitorExample>();
}

}  // namespace vsglab::examples
