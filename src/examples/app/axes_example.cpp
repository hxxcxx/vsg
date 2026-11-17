/**
 * @file axes_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgaxes
 *
 * 角落方位 Gizmo（Blender/3ds Max 同款）：主视口照常显示模型，右下角一个小坐标轴
 * 随主相机旋转实时同步指向。三个知识点——
 *   1. RotationTrackingMatrix：自定义 ViewMatrix 子类，transform() 只返回被跟踪
 *      相机视图矩阵的"旋转分量"（丢弃平移）——Gizmo 只跟随朝向、不跟随位置；
 *   2. createArrow/createGizmo：用 Builder 的圆柱+圆锥+球拼出三色坐标轴，
 *      含"把 +Z 朝向的箭头旋到任意方向"的四元数技巧；
 *   3. 两个 View 共用一个视口：Gizmo 的正交相机通过 left/right/bottom/top
 *      的偏移把自己"挤"到视口角落。
 *
 * 与原版的差异：
 * - 原版要求命令行传模型；试验场改为参数（默认 lz）。
 * - 去掉了 -f（帧数上限）；注释掉的 setRenderPass 支线未移植。
 *
 * @author hxxcxx
 * @date 2026-11-17
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

// A rotation tracking view matrix. It returns the rotation component
// of another viewMatrix.
class RotationTrackingMatrix : public vsg::Inherit<vsg::ViewMatrix, RotationTrackingMatrix> {
public:
    RotationTrackingMatrix(vsg::ref_ptr<vsg::ViewMatrix> parentTransform) : parentTransform_(parentTransform) {}

    // The transform() returns the rotation part of the tracked matrix
    vsg::dmat4 transform(const vsg::dvec3&) const override {
        vsg::dvec3 translation, scale;
        vsg::dquat rotation;
        vsg::decompose(parentTransform_->transform(),
                       // output
                       translation, rotation, scale);

        return vsg::rotate(rotation);
    }

private:
    vsg::ref_ptr<vsg::ViewMatrix> parentTransform_;
};

// Create an arrow with the back at pos and pointing in the direction of dir
// Place a cone at the end of the arrow with the color color
vsg::ref_ptr<vsg::Node> createArrow(vsg::vec3 pos, vsg::vec3 dir, vsg::vec4 color) {
    vsg::ref_ptr<vsg::Group> arrow = vsg::Group::create();

    vsg::Builder builder;
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;

    geomInfo.color = vsg::vec4{ 1, 1, 1, 1 };
    geomInfo.position = pos;
    geomInfo.transform = vsg::translate(0.0f, 0.0f, 0.5f);

    // If we don't point in the z-direction, then rotate the arrow
    if (vsg::length(vsg::cross(vsg::vec3{ 0, 0, 1 }, dir)) > 0.0001f) {
        vsg::vec3 axis = vsg::cross(vsg::vec3{ 0, 0, 1 }, dir);
        float angle = acos(vsg::dot(vsg::vec3{ 0, 0, 1 }, dir));
        geomInfo.transform = vsg::rotate(angle, axis) * geomInfo.transform;
    }

    auto axisTransform = geomInfo.transform;
    geomInfo.transform = geomInfo.transform * vsg::scale(0.1f, 0.1f, 1.0f);

    // Rotate geomInfo from pos in the direction of dir
    auto node = builder.createCylinder(geomInfo, stateInfo);
    arrow->addChild(node);

    // The cone
    geomInfo.color = color;
    // This would have been cleaner with a pre_translate transform
    geomInfo.transform = vsg::scale(0.3f, 0.3f, 0.3f) * axisTransform * vsg::translate(0.0f, 0.0f, 1.0f / 0.3f);
    node = builder.createCone(geomInfo, stateInfo);
    arrow->addChild(node);

    return arrow;
}

// Create three arrows of the coordinate axes
vsg::ref_ptr<vsg::Node> createGizmo() {
    vsg::ref_ptr<vsg::Group> gizmo = vsg::Group::create();

    gizmo->addChild(createArrow(vsg::vec3{ 0, 0, 0 }, vsg::vec3{ 1, 0, 0 }, vsg::vec4{ 1, 0, 0, 1 }));
    gizmo->addChild(createArrow(vsg::vec3{ 0, 0, 0 }, vsg::vec3{ 0, 1, 0 }, vsg::vec4{ 0, 1, 0, 1 }));
    gizmo->addChild(createArrow(vsg::vec3{ 0, 0, 0 }, vsg::vec3{ 0, 0, 1 }, vsg::vec4{ 0, 0, 1, 1 }));

    vsg::Builder builder;
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    geomInfo.color = vsg::vec4{ 1, 1, 1, 1 };
    geomInfo.transform = vsg::scale(0.1f, 0.1f, 0.1f);

    auto sphere = builder.createSphere(geomInfo, stateInfo);
    gizmo->addChild(sphere);

    return gizmo;
}

// Create a tracking overlay with a axes view that shows the orientation
// of the main camera view matrix
vsg::ref_ptr<vsg::View> createAxesView(vsg::ref_ptr<vsg::Camera> camera, double aspectRatio) {
    auto viewMat = RotationTrackingMatrix::create(camera->viewMatrix);

    // Place the gizmo in the view port by modifying its ortho camera x
    // and y limits
    double camWidth = 10;
    double camXOffs = -8;
    double camYOffs = -8;
    auto ortho = vsg::Orthographic::create(camXOffs, camXOffs + camWidth,                                // left, right
                                           camYOffs / aspectRatio, (camYOffs + camWidth) / aspectRatio,  // bottom, top
                                           -1000, 1000);                                                 // near, far

    auto gizmoCamera = vsg::Camera::create(ortho, viewMat, camera->viewportState);

    auto scene = vsg::Group::create();
    scene->addChild(createGizmo());

    auto view = vsg::View::create(gizmoCamera);
    view->addChild(vsg::createHeadlight());
    view->addChild(scene);
    return view;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width,
                                               uint32_t height) {
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                nearFarRatio * radius, radius * 4.5);

    auto viewportstate = vsg::ViewportState::create(x, y, width, height);

    return vsg::Camera::create(perspective, lookAt, viewportstate);
}

class AxesExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto scenegraph = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!scenegraph)
            throw vsg::Exception{ "Please specify a valid model file." };

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        uint32_t width = window->extent2D().width;
        uint32_t height = window->extent2D().height;
        double aspectRatio = (double) width / height;

        auto renderGraph = vsg::RenderGraph::create(window);

        // create view
        auto camera = createCameraForScene(scenegraph, 0, 0, width, height);
        auto view = vsg::View::create(camera);
        view->addChild(vsg::createHeadlight());
        view->addChild(scenegraph);
        renderGraph->addChild(view);
        renderGraph->addChild(createAxesView(camera, aspectRatio));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::CommandGraph::create(window);

        commandGraph->addChild(renderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

private:
    FileParam* model_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(AxesExample, "vsgaxes", "app",
                        "Corner orientation gizmo (Blender-style): a custom RotationTrackingMatrix view matrix "
                        "returns only the rotation of the main camera, so the Builder-made RGB axis triad follows "
                        "your view direction in the corner while the model fills the main viewport.");

}  // namespace vsglab::examples
