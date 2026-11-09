/**
 * @file example_registry.h
 * @brief 示例登记：每个示例在自己的 .cpp 末尾登记一次，界面据此列出全部示例
 *
 *   VSGLAB_REGISTER_EXAMPLE(HelloWorldExample, "vsghelloworld", "app", "一句话说明");
 *
 * 清单只记录"如何创建"，界面上每次创建实例都调用 create() 得到一个新对象；
 * 排列顺序取自 vsgExamples（见 example_registry.cpp 中的课程表），与登记先后无关。
 * 登记发生在程序启动时（静态初始化），因此示例源文件必须直接编进可执行文件，
 * 不能放进静态库（链接器会丢弃没有被引用的目标文件）。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/console_example.h"
#include "framework/example.h"
#include "framework/window_example.h"

#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

namespace vsglab::framework {

struct ExampleDescriptor {
    ExampleInfo info;
    std::function<std::unique_ptr<Example>()> create;
};

/// 全部已登记的示例，按 vsgExamples 的顺序排列
const std::vector<ExampleDescriptor>& exampleDescriptors();

/// 登记一个示例（由 VSGLAB_REGISTER_EXAMPLE 调用）
bool registerExample(ExampleDescriptor descriptor);

template <typename T>
bool registerExample(ExampleInfo info) {
    static_assert(std::is_base_of_v<ConsoleExample, T> != std::is_base_of_v<WindowExample, T>,
                  "an example must derive from exactly one of ConsoleExample / WindowExample");
    return registerExample(
            ExampleDescriptor{ std::move(info), []() -> std::unique_ptr<Example> { return std::make_unique<T>(); } });
}

}  // namespace vsglab::framework

/// 在示例 .cpp 中登记示例：类型、原版目录名、原版分类、一句话说明
#define VSGLAB_REGISTER_EXAMPLE(Type, id, category, brief)       \
    [[maybe_unused]] static const bool kVsglabRegistered##Type = \
            ::vsglab::framework::registerExample<Type>(::vsglab::framework::ExampleInfo{ id, category, brief })
