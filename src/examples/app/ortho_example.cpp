/**
 * @file ortho_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgortho
 *
 * vsg::Orthographic 正交投影：把 helloworld 的 Perspective 换成平行投影——
 * 没有"近大远小"，等大物体在任何距离投影尺寸相同（CAD/工程图的默认视角）。
 * halfWidth/halfHeight 按窗口长边与纵横比分配，保证模型不变形。
 *
 * 与原版的差异：
 * - 原版要求命令行传模型；试验场改为参数（默认 lz）。
 * - 去掉了 -f（帧数上限）。窗口标题由宿主提供。
 * - 原版窗口拉伸后投影不跟随更新（Orthographic 无 aspectRatio 字段），
 *   试验场行为一致；恢复窗口比例即可还原。
 *
 * @author hxxcxx
 * @date 2026-11-17
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

class OrthoExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "Please specify a valid 3d model file." };

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        double aspectRatio =
                static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);
        double halfDim = radius * 1.1;
        double halfHeight, halfWidth;
        if (window->extent2D().width > window->extent2D().height) {
            halfHeight = halfDim;
            halfWidth = halfDim * aspectRatio;
        } else {
            halfWidth = halfDim;
            halfHeight = halfDim / aspectRatio;
        }
        auto projection = vsg::Orthographic::create(-halfWidth, halfWidth, -halfHeight, halfHeight,
                                                    nearFarRatio * radius, radius * 4.5);

        auto camera = vsg::Camera::create(projection, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

private:
    FileParam* model_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(OrthoExample, "vsgortho", "app",
                        "vsg::Orthographic: swap the perspective for a parallel projection — no foreshortening, "
                        "equal-sized objects project equally at any distance (the CAD/engineering view).");

}  // namespace vsglab::examples
