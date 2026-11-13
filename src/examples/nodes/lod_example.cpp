/**
 * @file lod_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsglod
 *
 * vsg::LOD 节点：按子图包围球在屏幕上的占比在多个子档间切换。
 * 场景用 vsg::Builder 生成锥/柱/球，摆了四个 LOD 作演示：
 *   1) 远处空 → 柱 → 近处锥（双档互换）
 *   2) 球 → 近处锥
 *   3) 远处空 → 近处锥（单档浮现）
 *   4) 球 → 近处空（走近反而消失）
 * 渲染图采用显式 View → RenderGraph → CommandGraph 组合，
 * 对照 vsghelloworld 用的 createCommandGraphForView 便捷函数——手工展开就是这三层。
 *
 * 与原版的差异：
 * - 去掉了 -t / --st / -f / --fps / --duration（基准与自动退出）
 * - 去掉了 --ga / --profiler / --tracy（检测与剖析，留待 utils/vsginstrumentation）
 * - 去掉了 --rh / --orh（ResourceHints 读写）、-p（相机动画，data 无动画文件）、
 *   -o（写场景）、--inherit（inheritedState）
 * - --direction / --angleSubtended 光照参数固定为原版默认值，未做成界面参数
 * - 原版 Perspective 纵横比硬编码 1.0，这里按窗口实际尺寸计算（嵌入视口宽高比不定）
 * - 原版写死在源码里的 lodMinScreenRatio（0.01）做成界面参数（Restart）：
 *   调大它，各档切换时机提前；观察四个 LOD 的出现/消失距离随之变化
 *
 * @author hxxcxx
 * @date 2026-11-13
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::DoubleParam;
using framework::ParamSet;
using framework::WindowContext;

vsg::ref_ptr<vsg::Node> createTestScene(const vsg::ref_ptr<vsg::Options>& options, double lodMinScreenRatio) {
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    geomInfo.color.set(1.0f, 1.0f, 0.5f, 1.0f);
    vsg::StateInfo stateInfo;

    auto node = builder->createCone(geomInfo, stateInfo);
    auto node2 = builder->createCylinder(geomInfo, stateInfo);

    vsg::ComputeBounds cb;
    node->accept(cb);
    node2->accept(cb);
    auto bb = cb.bounds;

    // shows empty, then node2, then node when approaching
    auto lod = vsg::LOD::create();
    lod->bound = vsg::dsphere((bb.min + bb.max) * 0.5, vsg::length(bb.max - bb.min));
    lod->addChild(vsg::LOD::Child{ lodMinScreenRatio + 1, node });
    lod->addChild(vsg::LOD::Child{ lodMinScreenRatio, node2 });
    scene->addChild(lod);

    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(1.0f, 0.5f, 1.0f, 1.0f);
    auto node3 = builder->createCone(geomInfo, stateInfo);
    auto node4 = builder->createSphere(geomInfo, stateInfo);

    vsg::ComputeBounds cb2;
    node3->accept(cb2);
    node4->accept(cb2);
    auto bb2 = cb2.bounds;

    // should show node4, then node3 when approaching
    auto lod2 = vsg::LOD::create();
    lod2->bound = vsg::dsphere((bb2.min + bb2.max) * 0.5, vsg::length(bb2.max - bb2.min));
    lod2->addChild(vsg::LOD::Child{ lodMinScreenRatio, node3 });
    lod2->addChild(vsg::LOD::Child{ 0.0, node4 });
    scene->addChild(lod2);

    // should show empty, then node5 when approaching
    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(0.5f, 1.0f, 1.0f, 1.0f);
    auto node5 = builder->createCone(geomInfo, stateInfo);
    vsg::ComputeBounds cb3;
    node5->accept(cb3);
    auto bb3 = cb3.bounds;

    auto lod3 = vsg::LOD::create();
    lod3->bound = vsg::dsphere((bb3.min + bb3.max) * 0.5, vsg::length(bb3.max - bb3.min));
    lod3->addChild(vsg::LOD::Child{ lodMinScreenRatio + 0.5, node5 });
    scene->addChild(lod3);

    // should show for node7, then empty when approaching
    geomInfo.position += geomInfo.dx * 1.5f;
    geomInfo.color.set(0.5f, 0.5f, 1.0f, 1.0f);
    auto node6 = vsg::Group::create();
    auto node7 = builder->createSphere(geomInfo, stateInfo);
    vsg::ComputeBounds cb4;
    node6->accept(cb4);
    node7->accept(cb4);
    auto bb4 = cb4.bounds;

    auto lod4 = vsg::LOD::create();
    lod4->bound = vsg::dsphere((bb4.min + bb4.max) * 0.5, vsg::length(bb4.max - bb4.min));
    lod4->addChild(vsg::LOD::Child{ lodMinScreenRatio + 1, node6 });
    lod4->addChild(vsg::LOD::Child{ lodMinScreenRatio, node7 });
    scene->addChild(lod4);

    auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    vsg::info("createTestScene() extents = ", bounds);

    return scene;
}

class LodExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        lod_ratio_ = params.addDouble("Min screen ratio", 0.01, 0.001, 0.5, 0.005, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto scene = createTestScene(options, lod_ratio_->value());

        // compute the bounds of the scene graph to help position camera
        auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
        auto viewingDistance = vsg::length(bounds.max - bounds.min) * 2.0;

        vsg::ref_ptr<vsg::LookAt> lookAt;
        {
            vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre,
                                         vsg::dvec3(0.0, 0.0, 1.0));
        }

        auto group = vsg::Group::create();
        group->addChild(scene);

        // 平行光：Builder 的 Phong 几何需要光照
        // 注：原版 color.set(...) 传了 4 个参数，而本版 vsg::Light::color 是 vec3（三分量），
        // 该行本就编不过；默认白色即所需，删去
        auto directionalLight = vsg::DirectionalLight::create();
        directionalLight->name = "directional";
        directionalLight->intensity = 0.95f;
        directionalLight->direction.set(0.0, 0.0, -1.0);
        group->addChild(directionalLight);

        scene = group;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        auto perspective = vsg::Perspective::create(
                30, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 1.0,
                1000);

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add the camera and scene graph to View
        auto view = vsg::View::create();
        view->camera = camera;
        view->addChild(scene);

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        // 显式组合：View（相机+场景）→ RenderGraph（渲染目标与流程）→ CommandGraph（提交单元）
        auto renderGraph = vsg::RenderGraph::create(window, view);
        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

private:
    DoubleParam* lod_ratio_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(LodExample, "vsglod", "nodes",
                        "vsg::LOD: switch between child graphs by their on-screen size ratio. Four LODs demo "
                        "swapping, appearing on approach, and even vanishing up close; geometry from vsg::Builder "
                        "under a directional light. Render graph is assembled explicitly "
                        "(View → RenderGraph → CommandGraph).");

}  // namespace vsglab::examples
