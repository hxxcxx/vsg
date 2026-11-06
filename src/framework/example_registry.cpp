#include "framework/example_registry.h"

// 各示例在 src/examples/<category>/<类名 snake_case>.cpp 中定义工厂函数
namespace vsglab::examples {

std::unique_ptr<framework::Example> makeHelloWorldExample();

}  // namespace vsglab::examples

namespace vsglab::framework {

std::vector<std::unique_ptr<Example>> createExamples() {
    std::vector<std::unique_ptr<Example>> result;

    // --- app ---
    result.push_back(examples::makeHelloWorldExample());

    return result;
}

}  // namespace vsglab::framework
