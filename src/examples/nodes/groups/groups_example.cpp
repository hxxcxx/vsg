/**
 * @file groups_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsggroups
 *
 * 四种节点类型（vsg::Group / vsg::QuadGroup / shared_ptr 对照组两种）构建同一棵满四叉树，
 * 计时构建、遍历、析构三个阶段并给出吞吐量——回答"Group 与 QuadGroup 到底差多少"。
 * 遍历器三选一：Visitor（默认）/ ConstVisitor / RecordTraversal。
 *
 * 与原版的差异：
 * - 去掉了 --std / --allocator / --objects / --nodes / --data / --unit：它们改动进程级
 *   vsg::Allocator（同 vsgallocator 的处理），多实例试验场无法按实例切换；对照组的
 *   StdAllocator 类随之不再需要。
 * - 去掉了 -o（把树写成 .vsgt 后退出）：属于序列化支线，留到 io 分类。
 * - 输出文件路径改由宿主选项提供（-i 加载外部模型作为被测场景）。
 *
 * @author hxxcxx
 * @date 2026-11-12
 */

#include "examples/nodes/groups/shared_ptr_node.h"
#include "framework/console_example.h"
#include "framework/example_registry.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <memory>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::ChoiceParam;
using framework::ConsoleContext;
using framework::FileParam;
using framework::IntParam;
using framework::ParamSet;

class VsgVisitor : public vsg::Visitor {
public:
    uint64_t numNodes = 0;

    using Visitor::apply;

    void apply(vsg::Object& object) final {
        ++numNodes;
        object.traverse(*this);
    }

    void apply(vsg::Group& group) final {
        ++numNodes;
        group.traverse(*this);
    }

    void apply(vsg::QuadGroup& group) final {
        ++numNodes;
        group.traverse(*this);
    }
};

class VsgConstVisitor : public vsg::ConstVisitor {
public:
    uint64_t numNodes = 0;

    using ConstVisitor::apply;

    void apply(const vsg::Object& object) final {
        ++numNodes;
        object.traverse(*this);
    }

    void apply(const vsg::Group& group) final {
        ++numNodes;
        group.traverse(*this);
    }

    void apply(const vsg::QuadGroup& group) final {
        ++numNodes;
        group.traverse(*this);
    }
};

class ExperimentVisitor : public groups::SharedPtrVisitor {
public:
    uint64_t numNodes = 0;

    using SharedPtrVisitor::apply;

    void apply(groups::SharedPtrNode& object) final {
        ++numNodes;
        object.traverse(*this);
    }

    void apply(groups::SharedPtrGroup& group) final {
        ++numNodes;
        group.traverse(*this);
    }

    void apply(groups::SharedPtrQuadGroup& group) final {
        ++numNodes;
        group.traverse(*this);
    }
};

vsg::ref_ptr<vsg::Node> createVsgQuadTree(uint64_t numLevels, uint64_t& numNodes, uint64_t& numBytes) {
    if (numLevels == 0) {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);
        return vsg::Node::create();
    }

    auto t = vsg::Group::create(4);

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::Group) + 4 * sizeof(vsg::ref_ptr<vsg::Node>);

    t->children[0] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[1] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[2] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[3] = createVsgQuadTree(numLevels, numNodes, numBytes);

    return t;
}

vsg::ref_ptr<vsg::Node> createFixedQuadTree(uint64_t numLevels, uint64_t& numNodes, uint64_t& numBytes) {
    if (numLevels == 0) {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);
        return vsg::Node::create();
    }

    auto t = vsg::QuadGroup::create();

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::QuadGroup);

    t->children = {
        { createFixedQuadTree(numLevels, numNodes, numBytes), createFixedQuadTree(numLevels, numNodes, numBytes),
          createFixedQuadTree(numLevels, numNodes, numBytes), createFixedQuadTree(numLevels, numNodes, numBytes) }
    };

    return t;
}

std::shared_ptr<groups::SharedPtrNode> createSharedPtrQuadTree(uint64_t numLevels, uint64_t& numNodes,
                                                               uint64_t& numBytes) {
    if (numLevels == 0) {
        numNodes += 1;
        numBytes += sizeof(groups::SharedPtrNode);
        return std::allocate_shared<groups::SharedPtrNode>(vsg::allocator_affinity_nodes<groups::SharedPtrNode>());
    }

    std::shared_ptr<groups::SharedPtrGroup> t =
            std::allocate_shared<groups::SharedPtrGroup>(vsg::allocator_affinity_nodes<groups::SharedPtrNode>(), 4);

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(groups::SharedPtrGroup);

    t->setChild(0, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(1, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(2, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(3, createSharedPtrQuadTree(numLevels, numNodes, numBytes));

    return t;
}

std::shared_ptr<groups::SharedPtrNode> createSharedPtrFixedQuadTree(uint64_t numLevels, uint64_t& numNodes,
                                                                    uint64_t& numBytes) {
    if (numLevels == 0) {
        numNodes += 1;
        numBytes += sizeof(groups::SharedPtrNode);
        return std::allocate_shared<groups::SharedPtrNode>(vsg::allocator_affinity_nodes<groups::SharedPtrNode>());
    }

    std::shared_ptr<groups::SharedPtrQuadGroup> t =
            std::allocate_shared<groups::SharedPtrQuadGroup>(vsg::allocator_affinity_nodes<groups::SharedPtrNode>());

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(groups::SharedPtrQuadGroup);

    t->setChild(0, createSharedPtrFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(1, createSharedPtrFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(2, createSharedPtrFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(3, createSharedPtrFixedQuadTree(numLevels, numNodes, numBytes));

    return t;
}

class GroupsExample : public framework::ConsoleExample {
public:
    void declareParams(ParamSet& params) override {
        num_levels_ = params.addInt("Levels (-l)", 11, 0, 12, Apply::Restart);
        num_traversals_ = params.addInt("Traversals (-t)", 10, 1, 1000, Apply::Restart);
        type_ = params.addChoice("Node type (--type)",
                                 { "vsg::Group", "vsg::QuadGroup", "SharedPtrGroup", "SharedPtrQuadGroup" },
                                 "vsg::Group", Apply::Restart);
        visitor_ = params.addChoice("Visitor", { "Visitor", "ConstVisitor (-c)", "RecordTraversal (-d)" }, "Visitor",
                                    Apply::Restart);
        input_ = params.addFile("Input scene (-i, empty = build tree)", "", "Models (*.vsgt *.vsgb)",
                                { "", "models/lz.vsgt" }, Apply::Restart);
        quiet_ = params.addBool("Quiet (-q)", false, Apply::Live);
    }

    void run(ConsoleContext& ctx) override {
        const uint64_t numLevels = static_cast<uint64_t>(num_levels_->value());
        const uint64_t numTraversals = static_cast<uint64_t>(num_traversals_->value());
        const std::string& type = type_->value();
        const std::string& visitorMode = visitor_->value();
        const std::string& inputFilename = input_->value();

        using clock = std::chrono::high_resolution_clock;

        clock::time_point start = clock::now();

        vsg::ref_ptr<vsg::Node> vsg_root;
        std::shared_ptr<groups::SharedPtrNode> shared_root;
        uint64_t numNodes = 0;
        uint64_t numBytes = 0;

        if (!inputFilename.empty()) {
            vsg_root = vsg::read_cast<vsg::Node>(inputFilename, ctx.options());
            if (!vsg_root)
                throw vsg::Exception{ "Warning: file not loaded : " + inputFilename };
        } else {
            if (type == "vsg::Group")
                vsg_root = createVsgQuadTree(numLevels, numNodes, numBytes);
            if (type == "vsg::QuadGroup")
                vsg_root = createFixedQuadTree(numLevels, numNodes, numBytes);
            if (type == "SharedPtrGroup")
                shared_root = createSharedPtrQuadTree(numLevels, numNodes, numBytes);
            if (type == "SharedPtrQuadGroup")
                shared_root = createSharedPtrFixedQuadTree(numLevels, numNodes, numBytes);
        }

        if (!vsg_root && !shared_root)
            throw vsg::Exception{ "Error invalid type=" + type };

        clock::time_point after_construction = clock::now();

        uint64_t numNodesVisited = 0;
        if (vsg_root) {
            if (visitorMode == "RecordTraversal (-d)") {
                auto recordTraversal = vsg::RecordTraversal::create();
                std::cout << "using RecordTraversal" << std::endl;
                for (uint64_t i = 0; i < numTraversals; ++i) {
                    vsg_root->accept(*recordTraversal);
                }
            } else if (visitorMode == "ConstVisitor (-c)") {
                // 直接继承 vsg::ConstVisitor 的类没有自己的 create()（CRTP 的 create 返回基类），按原版用 new
                vsg::ref_ptr<VsgConstVisitor> constVisitor(new VsgConstVisitor);
                std::cout << "using VsgConstVisitor" << std::endl;
                for (uint64_t i = 0; i < numTraversals; ++i) {
                    vsg_root->accept(*constVisitor);
                    numNodesVisited += constVisitor->numNodes;
                    constVisitor->numNodes = 0;
                }
            } else {
                vsg::ref_ptr<VsgVisitor> visitor(new VsgVisitor);
                std::cout << "using VsgVisitor" << std::endl;
                for (uint64_t i = 0; i < numTraversals; ++i) {
                    vsg_root->accept(*visitor);
                    numNodesVisited += visitor->numNodes;
                    visitor->numNodes = 0;
                }
            }
        } else if (shared_root) {
            ExperimentVisitor experimentVisitor;
            for (uint64_t i = 0; i < numTraversals; ++i) {
                shared_root->accept(experimentVisitor);
                numNodesVisited += experimentVisitor.numNodes;
                experimentVisitor.numNodes = 0;
            }
        }

        clock::time_point after_traversal = clock::now();

        vsg_root = 0;
        shared_root = 0;

        clock::time_point after_destruction = clock::now();

        if (!quiet_->value()) {
            std::cout << "type : " << type << std::endl;
            std::cout << "numNodes : " << numNodes << std::endl;
            std::cout << "numBytes : " << numBytes << std::endl;
            std::cout << "average node size : " << double(numBytes) / double(numNodes) << std::endl;
            std::cout << "numNodesVisited : " << numNodesVisited << std::endl;
            if (!inputFilename.empty())
                std::cout << "read time : " << std::chrono::duration<double>(after_construction - start).count()
                          << std::endl;
            else
                std::cout << "construction time : " << std::chrono::duration<double>(after_construction - start).count()
                          << std::endl;
            std::cout << "traversal time : "
                      << std::chrono::duration<double>(after_traversal - after_construction).count() << std::endl;
            std::cout << "destruction time : "
                      << std::chrono::duration<double>(after_destruction - after_traversal).count() << std::endl;
            std::cout << "total time : " << std::chrono::duration<double>(after_destruction - start).count()
                      << std::endl;
            std::cout << std::endl;
            std::cout << "Nodes constructed per second : "
                      << double(numNodes) / std::chrono::duration<double>(after_construction - start).count()
                      << std::endl;
            std::cout << "Nodes visited per second     : "
                      << double(numNodesVisited) /
                                 std::chrono::duration<double>(after_traversal - after_construction).count()
                      << std::endl;
            std::cout << "Nodes destructed per second : "
                      << double(numNodes) / std::chrono::duration<double>(after_destruction - after_traversal).count()
                      << std::endl;
        }
    }

private:
    IntParam* num_levels_ = nullptr;
    IntParam* num_traversals_ = nullptr;
    ChoiceParam* type_ = nullptr;
    ChoiceParam* visitor_ = nullptr;
    FileParam* input_ = nullptr;
    BoolParam* quiet_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(GroupsExample, "vsggroups", "nodes",
                        "Benchmark quad trees built from four node types (vsg::Group, vsg::QuadGroup and two "
                        "std::shared_ptr variants), timing construction, traversal and destruction.");

}  // namespace vsglab::examples
