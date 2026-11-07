/**
 * @file example_registry.h
 * @brief 示例清单：顺序即 vsgExamples 的学习顺序，界面按 category 分组展示
 *
 * 清单只登记"如何创建"，界面上每次创建实例都调用 create() 得到一个新对象。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/example.h"

#include <functional>
#include <memory>
#include <vector>

namespace vsglab::framework {

struct ExampleDescriptor {
    ExampleInfo info;
    std::function<std::unique_ptr<Example>()> create;
};

/// 全部示例的描述（首次调用时构建）
const std::vector<ExampleDescriptor>& exampleDescriptors();

}  // namespace vsglab::framework
