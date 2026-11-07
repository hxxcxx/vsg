/**
 * @file memory_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgmemory
 *
 * 创建 N 个 vsg::Node，测量拷贝一个 ref_ptr<vsg::Object> 容器所需时间：
 * 体现 ref_ptr 侵入式引用计数的开销。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/example.h"

#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/nodes/Group.h>

#include <chrono>
#include <iostream>
#include <vector>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::ExampleContext;
using framework::ExampleInfo;
using framework::ExampleKind;
using framework::IntParam;
using framework::ParamSet;

class MemoryExample : public framework::Example {
public:
    ExampleInfo info() const override {
        return { "vsgmemory", "core",
                 "Create N vsg::Node objects and time copying a container of ref_ptr<vsg::Object>: "
                 "the cost of intrusive reference counting.",
                 ExampleKind::Console };
    }

    void declareParams(ParamSet& params) override {
        num_objects_ = params.addInt("Objects (-n)", 1000000, 1, 10000000, Apply::Restart);
    }

    void start(ExampleContext& /*ctx*/) override {
        auto numObjects = static_cast<unsigned int>(num_objects_->value());

        using Objects = std::vector<vsg::ref_ptr<vsg::Object>>;
        Objects objects;
        objects.reserve(numObjects);
        for (unsigned int i = 0; i < numObjects; ++i) {
            objects.push_back(vsg::Node::create());
        }

        Objects copy_objects(numObjects);

        using clock = std::chrono::high_resolution_clock;
        clock::time_point start = clock::now();

        copy_objects = objects;
        copy_objects.clear();

        std::cout << "Time to copy container with " << numObjects
                  << " objects : " << std::chrono::duration<double>(clock::now() - start).count() << " seconds."
                  << std::endl;
    }

private:
    IntParam* num_objects_ = nullptr;
};

}  // namespace

std::unique_ptr<framework::Example> makeMemoryExample() {
    return std::make_unique<MemoryExample>();
}

}  // namespace vsglab::examples
