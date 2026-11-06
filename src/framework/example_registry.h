/**
 * @file example_registry.h
 * @brief 示例清单：顺序即学习顺序，界面按 category 分组展示
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/example.h"

#include <memory>
#include <vector>

namespace vsglab::framework {

/// 创建全部示例实例（实例很轻，创建 viewer 发生在 start() 中）
std::vector<std::unique_ptr<Example>> createExamples();

}  // namespace vsglab::framework
