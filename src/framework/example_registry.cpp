#include "framework/example_registry.h"

// 各示例在 src/examples/<category>/<类名 snake_case>.cpp 中定义工厂函数
namespace vsglab::examples {

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
    static const std::vector<ExampleDescriptor> kDescriptors = {
        // --- app ---
        describe(&examples::makeHelloWorldExample),
    };
    return kDescriptors;
}

}  // namespace vsglab::framework
