/**
 * @file hello_world_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsghelloworld
 *
 * VSG 应用的最小完整流程：加载模型 → 创建窗口 → 按包围盒放置相机 →
 * Trackball 交互 → 创建 CommandGraph → compile → 渲染循环（由宿主驱动）。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::WindowContext;

class HelloWorldExample : public framework::WindowExample {
public:
    void declareParams(framework::ParamSet& params) override {
#ifdef vsgXchange_FOUND
        // openstreetmap.vsgt 需要在线下载 OSM 瓦片，网络不可达时必然失败，故不列入候选
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
#else
        model_ = params.addFile("Model", "models/lz.vsgt", "VSG Models (*.vsgt *.vsgb)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
#endif
        reset_view_ = params.addAction("Reset View");
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        // load the scene graph
        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "Failed to load model: " + model_->value() };

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
        home_view_ = vsg::LookAt::create(*lookAt);

        const double aspectRatio =
                static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel) {
            double horizonMountainHeight = 0.0;
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, aspectRatio, nearFarRatio,
                                                            horizonMountainHeight);
        } else {
            perspective = vsg::Perspective::create(30.0, aspectRatio, nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        // add trackball to control the Camera
        trackball_ = vsg::Trackball::create(camera, ellipsoidModel);
        viewer->addEventHandler(trackball_);

        // add the CommandGraph to render the scene
        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        // compile all Vulkan objects and transfer image, vertex and primitive data to GPU
        viewer->compile();

        ctx.viewer = viewer;
    }

    void onParamChanged(WindowContext& /*ctx*/, const framework::ParamBase& param) override {
        if (&param == reset_view_ && trackball_ && home_view_)
            trackball_->setViewpoint(vsg::LookAt::create(*home_view_), 0.5);
    }

    void stop(WindowContext& /*ctx*/) override {
        trackball_ = nullptr;
        home_view_ = nullptr;
    }

private:
    framework::FileParam* model_ = nullptr;
    framework::ActionParam* reset_view_ = nullptr;

    vsg::ref_ptr<vsg::Trackball> trackball_;
    vsg::ref_ptr<vsg::LookAt> home_view_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(HelloWorldExample, "vsghelloworld", "app",
                        "Load a model and browse it with a Trackball: the minimal complete VSG application. "
                        "Click into the viewport, then press Space to return to the home view.");

}  // namespace vsglab::examples
