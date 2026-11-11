/**
 * @file observer_ptr_experiment.cpp
 * @brief 试验场自编实验（非 vsgExamples 移植）：observer_ptr 的生命周期
 *
 * vsgpointer 演示了 ref_ptr（强引用），本实验补上另一半：observer_ptr（弱引用）。
 * 它不增加对象的引用计数，而是抓住对象的 Auxiliary：
 *   构造时 getOrCreateAuxiliary()（observer_ptr.h:40）——这就是 Auxiliary "按需创建" 的触发点之一
 *   判活 valid() = Auxiliary 活着 且 它的 getConnectedObject() 非空（observer_ptr.h:168）
 *   对象销毁时 signalConnectedObjectToBeDeleted() 把回指清空，Auxiliary 本身由 observer 续命
 *   提升为强引用时锁 Auxiliary 的互斥锁后再检查（observer_ptr.h:181）——那把 80 字节的锁的用武之地
 *
 * @author hxxcxx
 * @date 2026-11-11
 */

#include "framework/console_example.h"
#include "framework/example_registry.h"

#include <vsg/core/Auxiliary.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/nodes/Group.h>

#include <iostream>

namespace vsglab::examples {
namespace {

class ObserverPtrExperiment : public framework::ConsoleExample {
public:
    void run(framework::ConsoleContext& /*ctx*/) override {
        std::cout << std::boolalpha;

        std::cout << "1) create a node held by one ref_ptr\n";
        vsg::ref_ptr<vsg::Node> node = vsg::Node::create();
        std::cout << "   referenceCount = " << node->referenceCount() << ", auxiliary = " << node->getAuxiliary()
                  << " (nullptr: not needed yet)\n";

        std::cout << "2) create an observer_ptr from it\n";
        vsg::observer_ptr<vsg::Node> observer(node);
        std::cout << "   referenceCount = " << node->referenceCount() << " (unchanged: observing is weak)\n";
        std::cout << "   auxiliary = " << node->getAuxiliary() << " (created on demand by the observer)\n";
        std::cout << "   observer.valid() = " << observer.valid() << "\n";

        std::cout << "3) a second observer gets the same Auxiliary (equality compares it)\n";
        vsg::observer_ptr<vsg::Node> observer2(node);
        std::cout << "   observer == observer2 : " << (observer == observer2) << "\n";

        std::cout << "4) promote the observer to a strong ref_ptr (locks the Auxiliary mutex)\n";
        {
            vsg::ref_ptr<vsg::Node> strong = observer.ref_ptr();
            std::cout << "   referenceCount = " << node->referenceCount() << " (promoted: +1)\n";
        }
        std::cout << "   referenceCount = " << node->referenceCount() << " (strong ref_ptr gone)\n";

        std::cout << "5) drop the last strong ref_ptr: the node is destroyed\n";
        node = nullptr;
        std::cout << "   observer.valid() = " << observer.valid() << "\n";
        vsg::ref_ptr<vsg::Node> zombie = observer.ref_ptr();
        std::cout << "   promotion now yields " << zombie << " (safe, not the dead address)\n";
        std::cout << "   observer.get() = " << observer.get()
                  << " (raw pointer still dangles: only look, never touch)\n";

        std::cout << "6) the observer still keeps the Auxiliary alive; releasing the observers frees it too\n";
        observer = {};
        observer2 = {};
        std::cout << "   done.\n";
    }
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(ObserverPtrExperiment, "lab_observer_ptr", "core",
                        "Lab experiment (not from vsgExamples): the observer_ptr lifecycle — weak observing via the "
                        "object's Auxiliary, promoting to a strong ref_ptr, and what remains observable after the "
                        "object is gone.");

}  // namespace vsglab::examples
