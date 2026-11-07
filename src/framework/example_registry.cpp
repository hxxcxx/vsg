#include "framework/example_registry.h"

// 各示例在 src/examples/<category>/<类名 snake_case>.cpp 中定义工厂函数
namespace vsglab::examples {

// core
std::unique_ptr<framework::Example> makeAllocatorExample();
std::unique_ptr<framework::Example> makeArraysExample();
std::unique_ptr<framework::Example> makeMemoryExample();
std::unique_ptr<framework::Example> makePointerExample();
std::unique_ptr<framework::Example> makeTypesExample();
std::unique_ptr<framework::Example> makeValuesExample();
std::unique_ptr<framework::Example> makeVisitorExample();
std::unique_ptr<framework::Example> makeVisitorCustomTypeExample();

// app
std::unique_ptr<framework::Example> makeHelloWorldExample();

}  // namespace vsglab::examples

namespace vsglab::framework {
namespace {

using Factory = std::unique_ptr<Example> (*)();

ExampleDescriptor describe(Factory factory) {
    // 示例对象很轻（viewer 在 start() 中才创建），临时创建一个读取描述信息
    return { factory()->info(), factory };
}

}  // namespace

const std::vector<ExampleDescriptor>& exampleDescriptors() {
    // 分类顺序与 vsgExamples README 一致（core → nodes → maths → app → ...），
    // 分类内顺序与原版 CMakeLists 一致；界面上的分类按首次出现的顺序排列
    static const std::vector<ExampleDescriptor> kDescriptors = {
        // --- core ---
        describe(&examples::makeAllocatorExample),
        describe(&examples::makeArraysExample),
        describe(&examples::makeMemoryExample),
        describe(&examples::makePointerExample),
        describe(&examples::makeTypesExample),
        describe(&examples::makeValuesExample),
        describe(&examples::makeVisitorExample),
        describe(&examples::makeVisitorCustomTypeExample),

        // --- app ---
        describe(&examples::makeHelloWorldExample),
    };
    return kDescriptors;
}

}  // namespace vsglab::framework
