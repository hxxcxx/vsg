/**
 * @file annotation.h
 * @brief 往 Vulkan 命令流里发射调试标签的节点模板（VK_EXT_debug_utils）
 *
 * 移植自 vsgExamples/examples/nodes/vsgannotation/Annotation.h。
 * Annotation<ParentClass> 包装任意节点类：录制遍历进入该节点时发射
 * vkCmdBeginDebugUtilsLabelEXT（名字 + 颜色），离开时发射 End——
 * RenderDoc / nsight 等工具的命令流浏览器里会显示为带色命名区间。
 * 原版命名空间 experimental，此处移入 vsglab::examples::experimental。
 *
 * @author hxxcxx
 * @date 2026-11-15
 */
#pragma once

#include <vsg/app/RecordTraversal.h>
#include <vsg/core/Inherit.h>
#include <vsg/maths/vec4.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/vulkan.h>

#include <algorithm>
#include <string>

namespace vsglab::examples::experimental {

// A node which emits a debug label in the Vulkan command stream, which can be used by debugging
// utilities. For Example, RenderDoc will include these names in its event browser.
template <typename ParentClass>
class Annotation : public vsg::Inherit<ParentClass, Annotation<ParentClass>> {
public:
    template <typename... Args>
    Annotation(Args&&... args)
        : vsg::Inherit<ParentClass, Annotation<ParentClass>>(args...), debugColor(1.0f, 1.0f, 1.0f, 1.0f) {}

    vsg::vec4 debugColor;
    std::string annotation;

    using ParentClass::accept;

    void accept(vsg::RecordTraversal& visitor) const override {
        auto* commandBuffer = visitor.getCommandBuffer();
        auto extensions = commandBuffer->getDevice()->getInstance()->getExtensions();
        if (extensions->vkCmdBeginDebugUtilsLabelEXT && !annotation.empty()) {
            VkCommandBuffer cmdBuffer{ *commandBuffer };
            VkDebugUtilsLabelEXT markerInfo = {};
            markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            markerInfo.pLabelName = annotation.c_str();
            std::copy(&debugColor.value[0], &debugColor.value[4], &markerInfo.color[0]);

            extensions->vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &markerInfo);

            ParentClass::accept(visitor);

            extensions->vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
        } else {
            ParentClass::accept(visitor);
        }
    }
};

}  // namespace vsglab::examples::experimental
