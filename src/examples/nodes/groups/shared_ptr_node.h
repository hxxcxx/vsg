/**
 * @file shared_ptr_node.h
 * @brief vsggroups 示例的对照组：用 std::shared_ptr 实现的节点体系
 *
 * 移植自 vsgExamples/examples/nodes/vsggroups/SharedPtrNode.h。
 * 与 vsg::Group / vsg::QuadGroup 形成基准对照：同样的树，换成 shared_ptr 后的构建/遍历开销。
 * 原版命名空间 experimental，此处移入 vsglab::examples::groups。
 *
 * @author hxxcxx
 * @date 2026-11-12
 */
#pragma once

#include <array>
#include <memory>
#include <vector>

#include <vsg/core/Allocator.h>

namespace vsglab::examples::groups {

class SharedPtrVisitor;

class SharedPtrAuxiliary : public std::enable_shared_from_this<SharedPtrAuxiliary> {
public:
    SharedPtrAuxiliary() {}

protected:
    virtual ~SharedPtrAuxiliary() {}
};

class SharedPtrNode : public std::enable_shared_from_this<SharedPtrNode> {
public:
    SharedPtrNode() {}

    virtual void accept(SharedPtrVisitor& spv);
    virtual void traverse(SharedPtrVisitor& spv);

    virtual ~SharedPtrNode() {}

protected:
    std::shared_ptr<SharedPtrAuxiliary> _auxiliary;
};

class SharedPtrGroup;
class SharedPtrQuadGroup;

class SharedPtrVisitor {
public:
    virtual void apply(SharedPtrNode&) {}
    virtual void apply(SharedPtrGroup&) {}
    virtual void apply(SharedPtrQuadGroup&) {}
};

class SharedPtrGroup : public SharedPtrNode {
public:
    SharedPtrGroup() {}
    SharedPtrGroup(size_t size) : _children(size, nullptr) {}

    virtual void accept(SharedPtrVisitor& spv);
    virtual void traverse(SharedPtrVisitor& spv);

    using Children =
            std::vector<std::shared_ptr<SharedPtrNode>, vsg::allocator_affinity_nodes<std::shared_ptr<SharedPtrNode>>>;

    void setChild(std::size_t i, std::shared_ptr<SharedPtrNode> node) { _children[i] = std::move(node); }
    SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
    const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

    virtual ~SharedPtrGroup() {}

protected:
    Children _children;
};

class SharedPtrQuadGroup : public SharedPtrNode {
public:
    SharedPtrQuadGroup() {}

    virtual void accept(SharedPtrVisitor& spv);
    virtual void traverse(SharedPtrVisitor& spv);

    using Children = std::array<std::shared_ptr<SharedPtrNode>, 4>;

    void setChild(std::size_t i, std::shared_ptr<SharedPtrNode> node) { _children[i] = std::move(node); }
    SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
    const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

    virtual ~SharedPtrQuadGroup() {}

protected:
    Children _children;
};

}  // namespace vsglab::examples::groups
