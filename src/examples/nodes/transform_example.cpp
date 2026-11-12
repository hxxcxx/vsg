/**
 * @file transform_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgtransform
 *
 * vsg::MatrixTransform：同一个模型挂到三个变换节点下——左平移、绕中心缩放、平移+旋转，
 * 演示变换矩阵的组合顺序（vsg::translate * vsg::scale * vsg::rotate）与子图共享。
 * 相机定位使用 vsg::visit<ComputeBounds>() 便捷模板。
 *
 * 与原版的差异：
 * - 去掉了 -o（把组合后的场景写成文件后退出）：属于序列化支线，留到 io 分类。
 * - 原版创建了一个 vsg::View 但从未使用（commandGraph 直接用 scene），未移植。
 * - 原版在渲染循环结束后打印平均帧率，这里改在停止（stop()）时打印。
 * - 原版给 options 设置了 sharedObjects（读取去重），宿主选项未设置该项，行为一致只是不去重。
 *
 * @author hxxcxx
 * @date 2026-11-12
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::DoubleParam;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

class TransformExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        scale_ = params.addDouble("Scale (-s)", 3.0, 0.1, 100.0, 0.1, Apply::Restart);
        just_scale_ = params.addBool("Just scale (--js)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto model = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!model)
            throw vsg::Exception{ "Failed to load " + model_->value() };

        const double scale = scale_->value();

        // compute the bounds of the scene graph to help position camera
        auto bounds = vsg::visit<vsg::ComputeBounds>(model).bounds;
        vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
        double radius = vsg::length(bounds.max - bounds.min) * 0.6;

        auto scene = vsg::Group::create();

        if (!just_scale_->value()) {
            auto tm_1 = vsg::MatrixTransform::create();
            tm_1->matrix = vsg::translate(-radius * (0.75 + scale * 0.5), 0.0, 0.0);
            tm_1->addChild(model);
            scene->addChild(tm_1);
        }

        auto tm_2 = vsg::MatrixTransform::create();
        tm_2->matrix = vsg::translate(centre) * vsg::scale(scale, scale, scale) * vsg::translate(-centre);
        tm_2->addChild(model);
        scene->addChild(tm_2);

        if (!just_scale_->value()) {
            auto tm_3 = vsg::MatrixTransform::create();
            tm_3->matrix = vsg::translate(centre + vsg::dvec3(radius * (0.75 + scale * 0.5), 0.0, 0.0)) *
                           vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(-centre);
            tm_3->addChild(model);
            scene->addChild(tm_3);
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        // update bounds to new scene extent
        bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
        centre = (bounds.min + bounds.max) * 0.5;
        radius = vsg::length(bounds.max - bounds.min) * 0.6;

        // set up the camera
        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        double nearFarRatio = 0.001;
        auto perspective = vsg::Perspective::create(
                30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                nearFarRatio * radius, radius * 10.0);

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void stop(WindowContext& ctx) override {
        if (auto frameStamp = ctx.viewer ? ctx.viewer->getFrameStamp() : nullptr) {
            double numFramesCompleted = static_cast<double>(frameStamp->frameCount);
            auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - start_time_)
                                    .count();
            if (numFramesCompleted > 0.0)
                std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
        }
    }

private:
    FileParam* model_ = nullptr;
    DoubleParam* scale_ = nullptr;
    BoolParam* just_scale_ = nullptr;

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(TransformExample, "vsgtransform", "nodes",
                        "vsg::MatrixTransform: the same model under three transforms — translate, scale about the "
                        "centre, translate plus rotate — showing matrix composition order and subgraph sharing.");

}  // namespace vsglab::examples
