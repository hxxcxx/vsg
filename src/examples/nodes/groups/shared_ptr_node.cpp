/**
 * @file shared_ptr_node.cpp
 * @brief vsggroups 示例对照组的 accept/traverse 实现
 *
 * 移植自 vsgExamples/examples/nodes/vsggroups/SharedPtrNode.cpp。
 *
 * @author hxxcxx
 * @date 2026-11-12
 */

#include "examples/nodes/groups/shared_ptr_node.h"

namespace vsglab::examples::groups {

void SharedPtrNode::accept(SharedPtrVisitor& spv) {
    spv.apply(*this);
}

void SharedPtrNode::traverse(SharedPtrVisitor&) {
}

void SharedPtrGroup::accept(SharedPtrVisitor& spv) {
    spv.apply(*this);
}

void SharedPtrGroup::traverse(SharedPtrVisitor& spv) {
    for (const auto& child : _children)
        child->accept(spv);
}

void SharedPtrQuadGroup::accept(SharedPtrVisitor& spv) {
    spv.apply(*this);
}

void SharedPtrQuadGroup::traverse(SharedPtrVisitor& spv) {
    for (int i = 0; i < 4; ++i)
        _children[i]->accept(spv);
}

}  // namespace vsglab::examples::groups
