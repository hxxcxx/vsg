/**
 * @file layers_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsglayers
 *
 * vsg::Layer 节点 + binNumber：把子图划入不同渲染层，控制记录/提交顺序。
 * 模型放 bin 0，文字 HUD 放 bin 1 且在绘制前用 ClearAttachments 清空深度——
 * 无论模型与文字的深度关系如何，文字始终叠在模型之上。
 *
 * 与原版的差异：
 * - 去掉了 -o（写场景）、-f（帧数上限）
 * - 字体固定为 fonts/times.vsgb（原版 -f 参数，data/fonts 下有备选字体可自行改路径）
 * - 原版创建的 vsg::View 未被 commandGraph 使用（直接 createCommandGraphForView），未移植
 * - 原版在渲染循环结束后打印平均帧率，这里改在停止（stop()）时打印
 *
 * @author hxxcxx
 * @date 2026-11-13
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

class LayersExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        auto model = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!model)
            throw vsg::Exception{ "Failed to load " + model_->value() };

        // compute the bounds of the scene graph to help position camera
        auto bounds = vsg::visit<vsg::ComputeBounds>(model).bounds;
        vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
        double radius = vsg::length(bounds.max - bounds.min) * 0.6;

        auto scene = vsg::Group::create();

        auto font = vsg::read_cast<vsg::Font>("fonts/times.vsgb", options);
        if (!font)
            throw vsg::Exception{ "Failed to load fonts/times.vsgb" };

        auto layout = vsg::StandardLayout::create();
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->position = vsg::vec3(0.0f, 0.0f, 0.0f);
        layout->horizontal = vsg::vec3(1.0f, 0.0f, 0.0f);
        layout->vertical = vsg::vec3(0.0f, 0.0f, 1.0f);
        layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        layout->outlineWidth = 0.1f;

        auto text = vsg::Text::create();
        text->text = vsg::stringValue::create("Layer");
        text->font = font;
        text->layout = layout;
        text->setup(0, options);

        // clear the depth buffer before view2 gets rendered
        uint32_t width = window->extent2D().width;
        uint32_t height = window->extent2D().height;
        VkClearValue clearValue{};
        clearValue.depthStencil = { 0.0f, 0 };
        VkClearAttachment attachment{ VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearValue };
        VkClearRect rect{ VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ width, height } }, 0, 1 };
        auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{ attachment },
                                                              vsg::ClearAttachments::Rects{ rect });

        auto hud = vsg::Group::create();
        hud->addChild(clearAttachments);
        hud->addChild(text);

        // draw lower binNumber first
        auto layer_1 = vsg::Layer::create();
        layer_1->binNumber = 0;
        layer_1->value = 0;
        layer_1->child = model;

        // draw higher binNumber second
        auto layer_2 = vsg::Layer::create();
        layer_2->binNumber = 1;
        layer_2->value = 1;
        layer_2->child = hud;

        scene->addChild(layer_1);
        scene->addChild(layer_2);

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
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

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(LayersExample, "vsglayers", "nodes",
                        "vsg::Layer + binNumber: split subgraphs into render layers that control submission order "
                        "— the model goes to bin 0, a text HUD to bin 1 which first clears the depth buffer, so "
                        "the text always draws on top regardless of depth.");

}  // namespace vsglab::examples
