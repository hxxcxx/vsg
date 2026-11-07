/**
 * @file arrays_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgarrays
 *
 * vsg::Array 系列容器：按大小创建 floatArray / vec4Array，用 std::for_each 遍历赋值，
 * 以及用初始化列表构造 vec2Array / vec4Array。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/example.h"

#include <vsg/core/Array.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Visitor.h>
#include <vsg/io/stream.h>

#include <algorithm>
#include <iostream>

namespace vsglab::examples {
namespace {

using framework::ExampleContext;
using framework::ExampleInfo;
using framework::ExampleKind;

class ArraysExample : public framework::Example {
public:
    ExampleInfo info() const override {
        return { "vsgarrays", "core",
                 "vsg::Array containers: create floatArray / vec4Array by size, iterate with std::for_each, "
                 "and construct arrays from initializer lists.",
                 ExampleKind::Console };
    }

    void start(ExampleContext& /*ctx*/) override {
        auto floats = vsg::floatArray::create(10);

        std::cout << "floats.size() = " << floats->size() << std::endl;

        float value = 0.0f;
        std::for_each(floats->begin(), floats->end(), [&value](float& v) { v = value++; });

        std::for_each(floats->begin(), floats->end(), [](float& v) { std::cout << "   v[] = " << v << std::endl; });

        auto colours = vsg::vec4Array::create(40);
        vsg::vec4 colour(0.25, 0.5, 0.75, 1.0);
        for (std::size_t i = 0; i < colours->size(); ++i) {
            (*colours)[i] = colour;
            colour = vsg::vec4(colour.g, colour.b, colour.a, colour.r);
        }

        std::for_each(colours->begin(), colours->end(),
                      [](vsg::vec4& c) { std::cout << "   c[] = " << c << std::endl; });

        auto texCoords = vsg::vec2Array::create({ { 1.0f, 2.0f }, { 3.0f, 4.0f }, {} });

        std::cout << "texCoords.size() = " << texCoords->size() << std::endl;
        for (auto p : *texCoords) {
            std::cout << "    tc " << p.x << ", " << p.y << std::endl;
        }

        auto col = vsg::vec4Array::create({ {} });
        std::cout << "col.size() = " << col->size() << std::endl;
        for (auto c : *col) {
            std::cout << "    colour " << c.r << ", " << c.g << ", " << c.b << ", " << c.a << std::endl;
        }
    }
};

}  // namespace

std::unique_ptr<framework::Example> makeArraysExample() {
    return std::make_unique<ArraysExample>();
}

}  // namespace vsglab::examples
