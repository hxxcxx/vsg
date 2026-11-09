/**
 * @file alternate_visitor_custom_type.h
 * @brief vsgvisitorcustomtype 第二种方式：自定义节点重写 accept() 主动分派
 *
 * 移植自 vsgExamples/examples/core/vsgvisitorcustomtype/AlternateVisitorCustomType.h。
 * 节点在 accept() 中检查 visitor 是否为 AlternateCustomVisitorBase，是则调用对应的 apply()，
 * 否则按普通 Group 处理。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */
#pragma once

#include <vsg/core/Visitor.h>
#include <vsg/nodes/Group.h>

#include <iostream>
#include <string>

namespace vsglab::examples::visitor_custom_type {

class AlternateCustomGroupNode;
class AlternateCustomLODNode;

class AlternateCustomVisitorBase : public vsg::Inherit<vsg::Visitor, AlternateCustomVisitorBase> {
public:
    virtual void apply(AlternateCustomGroupNode& /*node*/) {}
    virtual void apply(AlternateCustomLODNode& /*node*/) {}
};

class AlternateCustomGroupNode : public vsg::Inherit<vsg::Group, AlternateCustomGroupNode> {
public:
    void accept(vsg::Visitor& visitor) override {
        if (auto acvb = visitor.cast<AlternateCustomVisitorBase>())
            acvb->apply(*this);
        else
            visitor.apply(*this);
    }

    std::string name = "alternate car";

protected:
    ~AlternateCustomGroupNode() = default;
};

class AlternateCustomLODNode : public vsg::Inherit<vsg::Group, AlternateCustomLODNode> {
public:
    void accept(vsg::Visitor& visitor) override {
        if (auto acvb = visitor.cast<AlternateCustomVisitorBase>())
            acvb->apply(*this);
        else
            visitor.apply(*this);
    }

    double maxDistance = 2.0;

protected:
    ~AlternateCustomLODNode() = default;
};

class AlternateVisitCustomTypes : public AlternateCustomVisitorBase {
public:
    void apply(vsg::Group& group) override {
        std::cout << "apply(Group& node)" << std::endl;
        group.traverse(*this);
    }

    void apply(AlternateCustomGroupNode& node) override {
        std::cout << "apply(AlternateCustomGroupNode& node) name = " << node.name << std::endl;
        node.traverse(*this);
    }

    void apply(AlternateCustomLODNode& node) override {
        std::cout << "apply(AlternateCustomLODNode& node) maxDistance = " << node.maxDistance << std::endl;
        node.traverse(*this);
    }
};

}  // namespace vsglab::examples::visitor_custom_type
