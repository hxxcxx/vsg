/**
 * @file pointer_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgpointer
 *
 * 对比 vsg::ref_ptr（侵入式引用计数）与 std::shared_ptr：
 * 打印对象与智能指针的大小、对齐，以及 vsg::Object / vsg::vec3 的内存布局。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/console_example.h"
#include "framework/example_registry.h"

#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/maths/vec3.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/QuadGroup.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>

namespace vsglab::examples {
namespace {

class SharedPtrAuxiliary : public std::enable_shared_from_this<SharedPtrAuxiliary> {
public:
    SharedPtrAuxiliary() {}

protected:
    virtual ~SharedPtrAuxiliary() {}
};

class SharedPtrNode : public std::enable_shared_from_this<SharedPtrNode> {
public:
    SharedPtrNode() {}

protected:
    virtual ~SharedPtrNode() {}

    std::shared_ptr<SharedPtrAuxiliary> _auxilary;
};

class SharedPtrQuadGroup : public SharedPtrNode {
public:
    SharedPtrQuadGroup() {}

    using Children = std::array<std::shared_ptr<SharedPtrNode>, 4>;

    void setChild(std::size_t i, SharedPtrNode* node) { _children[i] = node ? node->shared_from_this() : nullptr; }
    SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
    const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

    // protected:
    virtual ~SharedPtrQuadGroup() {}

    Children _children;
};

class PointerExample : public framework::ConsoleExample {
public:
    void run(framework::ConsoleContext& /*ctx*/) override {
        vsg::ref_ptr<vsg::QuadGroup> ref_node = vsg::QuadGroup::create();
        std::cout << "sizeof(Object)=" << sizeof(vsg::Object) << std::endl;
        std::cout << "sizeof(atomic_uint)=" << sizeof(std::atomic_uint) << std::endl;
        std::cout << "sizeof(atomic_ushort)=" << sizeof(std::atomic_ushort) << std::endl;
        std::cout << "sizeof(Auxiliary)=" << sizeof(vsg::Auxiliary) << std::endl;
        std::cout << "sizeof(vsg::ref_ptr)=" << sizeof(ref_node) << ", sizeof(QuadGroup)=" << sizeof(vsg::QuadGroup)
                  << std::endl;

        // 原版用 #if 1 / #else 演示：shared_from_this() 返回的是基类 shared_ptr，无法直接赋给派生类 shared_ptr
        std::shared_ptr<SharedPtrQuadGroup> shared_node(new SharedPtrQuadGroup);

        std::cout << "sizeof(std::shared_ptr)=" << sizeof(shared_node)
                  << ", sizeof(SharedPtrQuadGroup)=" << sizeof(SharedPtrQuadGroup) << std::endl;

        std::cout << std::endl << "Alignment int " << alignof(int) << std::endl;
        std::cout << "Alignment std::atomic_uint  " << alignof(std::atomic_uint) << std::endl;
        std::cout << "Alignment std::atomic_ushort  " << alignof(std::atomic_ushort) << std::endl;
        std::cout << "Alignment Object  " << alignof(vsg::Object) << std::endl;
        std::cout << "Alignment Object* " << alignof(vsg::Object*) << std::endl;
        std::cout << "Alignment Auxiliary  " << alignof(vsg::Auxiliary) << std::endl;
        std::cout << "Alignment Auxiliary* " << alignof(vsg::Auxiliary*) << std::endl;
        std::cout << "Alignment SharedPtrNode  " << alignof(SharedPtrNode) << std::endl;
        std::cout << "Alignment SharedPtrNode* " << alignof(SharedPtrNode*) << std::endl;

        std::cout << std::endl;
        std::cout << "std::is_standard_layout<vsg::Object> " << std::is_standard_layout<vsg::Object>::value
                  << std::endl;
        std::cout << "std::is_standard_layout<vsg::vec3> " << std::is_standard_layout<vsg::vec3>::value << std::endl;
        std::cout << "offsetof<vsg::vec3,x> " << offsetof(vsg::vec3, x) << std::endl;
        std::cout << "offsetof<vsg::vec3,y> " << offsetof(vsg::vec3, y) << std::endl;
        std::cout << "offsetof<vsg::vec3,z> " << offsetof(vsg::vec3, z) << std::endl;
    }
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(PointerExample, "vsgpointer", "core",
                        "Compare vsg::ref_ptr (intrusive reference counting) with std::shared_ptr: "
                        "object and pointer sizes, alignments, and the memory layout of vsg::Object / vsg::vec3.");

}  // namespace vsglab::examples
