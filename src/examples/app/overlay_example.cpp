/**
 * @file overlay_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgoverlay
 *
 * 一个窗口叠两个 View（真·屏幕叠加层）：view1 全屏画模型一，清深度后 view2
 * 全屏画模型二——两个独立的相机/场景/视口，后画的盖在前面（HUD 的标准做法，
 * 对照 vsglayers 的"同 View 改绘制顺序"路线）。灯光用 vsg::createHeadlight()
 * 挂进两个 View。Trackball 只操控 view1 的相机。
 *
 * 与原版的差异：
 * - 原版命令行传一或两个模型（一个时第二个读同一文件）；试验场改为两个文件
 *   参数，默认 lz + teapot。
 * - 注释掉的 setRenderPass 支线未移植。
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

class OverlayExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_1_ = params.addFile("Model 1 (view 1)", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                  { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        model_2_ = params.addFile("Model 2 (view 2)", "models/teapot.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                  { "models/teapot.vsgt", "models/lz.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto scenegraph = vsg::read_cast<vsg::Node>(model_1_->value(), options);
        if (!scenegraph)
            throw vsg::Exception{ "Failed to load model 1: " + model_1_->value() };
        auto scenegraph2 = vsg::read_cast<vsg::Node>(model_2_->value(), options);
        if (!scenegraph2)
            throw vsg::Exception{ "Failed to load model 2: " + model_2_->value() };

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        uint32_t width = window->extent2D().width;
        uint32_t height = window->extent2D().height;

        auto renderGraph = vsg::RenderGraph::create(window);

        // create view1
        auto camera = createCameraForScene(scenegraph, 0, 0, width, height);
        auto view1 = vsg::View::create(camera, scenegraph);
        renderGraph->addChild(view1);

        // clear the depth buffer before view2 gets rendered
        VkClearValue clearValue{};
        clearValue.depthStencil = { 0.0f, 0 };
        VkClearAttachment attachment{ VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearValue };
        VkClearRect rect{ VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ width, height } }, 0, 1 };
        auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{ attachment },
                                                              vsg::ClearAttachments::Rects{ rect });
        renderGraph->addChild(clearAttachments);

        // create view2
        auto secondary_camera = createCameraForScene(scenegraph2, 0, 0, width, height);
        auto view2 = vsg::View::create(secondary_camera, scenegraph2);
        renderGraph->addChild(view2);

        // add lighting
        auto headlight = vsg::createHeadlight();
        view1->addChild(headlight);
        view2->addChild(headlight);

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        // Trackball 只操控 view1 的相机（view2 是"钉在屏幕上的叠加层"）
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::CommandGraph::create(window);

        commandGraph->addChild(renderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

private:
    FileParam* model_1_ = nullptr;
    FileParam* model_2_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(OverlayExample, "vsgoverlay", "app",
                        "Two stacked Views in one window: view1 draws the main scene, then the depth buffer is "
                        "cleared and view2 draws a second scene on top with its own camera — the real screen-space "
                        "overlay route, compared with vsglayers' draw-order approach. Trackball drives view1 only.");

}  // namespace vsglab::examples
