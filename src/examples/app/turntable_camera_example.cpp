/**
 * @file turntable_camera_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgturntablecamera（含 Turntable.h/.cpp，
 *        Nolan Kramer 贡献，MIT 许可，版权头保留在源文件中）
 *
 * vsgviewer 的骨架 + 自定义相机控制器 Turntable：替换 Trackball 的另一种
 * 操纵器——WASDQE 六键姿态控制（左转/右转/俯仰/横滚）+ 方向键平移 + IO 前后
 * + 鼠标拖转/滚轮缩放，带惯性（throw）。打开视口后按住 W/S 等键体验。
 *
 * 与原版的差异：
 * - 模型由两个文件参数提供（原版命令行任意多个；第二个可清空跳过）。
 * - 去掉了 -t/--st/--fps、-c CPU 亲和性、--ga/--pr 检测、--ss、--rh/--orh、
 *   --ot、--rgb、--log-level/--log/--redirect-std/--ThreadLogger、--max-time。
 * - -p 相机动画、--dc、--mt、--load-levels/--maxPagedLOD/--nfr/--hmh、
 *   动画自动播放改为界面参数。
 *
 * @author hxxcxx
 * @date 2026-11-17
 */

#include "examples/app/turntable/turntable.h"
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
using framework::IntParam;
using framework::ParamSet;
using framework::WindowContext;

vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData, vsg::ref_ptr<vsg::Options> options) {
    auto builder = vsg::Builder::create();
    builder->options = options;
    vsg::StateInfo state;
    state.image = sourceData;
    state.lighting = false;
    vsg::GeometryInfo geom;
    geom.dy.set(0.0f, 0.0f, 1.0f);
    geom.dz.set(0.0f, -1.0f, 0.0f);
    return builder->createQuad(geom, state);
}

class TurntableCameraExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_1_ = params.addFile("Model 1", "models/lz.vsgt",
                                  "Models/Images (*.vsgt *.vsgb *.gltf *.glb *.obj *.png *.jpg)",
                                  { "models/lz.vsgt", "models/teapot.vsgt", "textures/VSGlogo.png" }, Apply::Restart);
        model_2_ = params.addFile("Model 2", "", "Models/Images (*.vsgt *.vsgb *.gltf *.glb *.obj *.png *.jpg)",
                                  { "", "models/teapot.vsgt" }, Apply::Restart);
        camera_path_ = params.addFile("Camera animation (-p)", "", "Animations (*.vsgt *.vsgb)", {}, Apply::Restart);
        auto_play_ = params.addBool("Auto play animation", true, Apply::Restart);
        load_levels_ = params.addInt("Preload levels (--load-levels)", 0, 0, 20, Apply::Restart);
        max_paged_lod_ = params.addInt("Max high-res PagedLOD (--maxPagedLOD)", 0, 0, 10000, Apply::Restart);
        near_far_ratio_ = params.addDouble("Near/far ratio (--nfr)", 0.001, 1e-6, 0.1, 0.001, Apply::Restart);
        horizon_ = params.addDouble("Horizon mountain height (--hmh)", 0.0, 0.0, 10000.0, 10.0, Apply::Restart);
        depth_clamp_ = params.addBool("Depth clamp (--dc)", false, Apply::Restart);
        multithreading_ = params.addBool("Multi-threaded record (--mt)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        if (depth_clamp_->value()) {
            std::cout << "Enabled depth clamp." << std::endl;
            auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
            deviceFeatures->get().samplerAnisotropy = VK_TRUE;
            deviceFeatures->get().depthClamp = VK_TRUE;
        }

        auto group = vsg::Group::create();
        for (FileParam* param : { model_1_, model_2_ }) {
            vsg::Path filename = param->value();
            if (filename.empty())
                continue;

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>()) {
                group->addChild(node);
            } else if (auto data = object.cast<vsg::Data>()) {
                if (auto textureGeometry = createTextureQuad(data, options)) {
                    group->addChild(textureGeometry);
                }
            } else if (object) {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
            } else {
                std::cout << "Unable to load file " << filename << std::endl;
            }
        }
        if (group->children.empty())
            throw vsg::Exception{ "Please specify a 3d model or image file." };

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (group->children.size() == 1)
            vsg_scene = group->children[0];
        else
            vsg_scene = group;

        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel) {
            perspective = vsg::EllipsoidPerspective::create(
                    lookAt, ellipsoidModel, 30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    near_far_ratio_->value(), horizon_->value());
        } else {
            perspective = vsg::Perspective::create(
                    30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    near_far_ratio_->value() * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, vsg::Path(camera_path_->value()), options);
        viewer->addEventHandler(cameraAnimation);
        if (auto_play_->value() && cameraAnimation->animation) {
            cameraAnimation->play();
        }

        // ★ 与 vsgviewer 的唯一本质区别：Turntable 替换 Trackball
        auto cameraController = turntable::Turntable::create(camera);
        viewer->addEventHandler(cameraController);

        if (load_levels_->value() > 0) {
            vsg::LoadPagedLOD loadPagedLOD(camera, load_levels_->value());
            auto startTime = vsg::clock::now();
            vsg_scene->accept(loadPagedLOD);
            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(vsg::clock::now() - startTime)
                                .count();
            std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        if (multithreading_->value())
            viewer->setupThreading();

        viewer->compile();

        if (max_paged_lod_->value() > 0) {
            for (auto& task : viewer->recordAndSubmitTasks) {
                if (task->databasePager)
                    task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs =
                            static_cast<uint32_t>(max_paged_lod_->value());
            }
        }

        if (auto_play_->value()) {
            auto animationGroups = vsg::visit<vsg::FindAnimations>(vsg_scene).animationGroups;
            for (auto ag : animationGroups) {
                if (!ag->animations.empty())
                    viewer->animationManager->play(ag->animations.front());
            }
        }

        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void stop(WindowContext& ctx) override {
        if (auto frameStamp = ctx.viewer ? ctx.viewer->getFrameStamp() : nullptr) {
            double duration =
                    std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - start_time_)
                            .count();
            if (frameStamp->frameCount > 0 && duration > 0.0)
                std::cout << "Average frame rate = " << (static_cast<double>(frameStamp->frameCount) / duration)
                          << std::endl;
        }
    }

private:
    FileParam* model_1_ = nullptr;
    FileParam* model_2_ = nullptr;
    FileParam* camera_path_ = nullptr;
    BoolParam* auto_play_ = nullptr;
    IntParam* load_levels_ = nullptr;
    IntParam* max_paged_lod_ = nullptr;
    DoubleParam* near_far_ratio_ = nullptr;
    DoubleParam* horizon_ = nullptr;
    BoolParam* depth_clamp_ = nullptr;
    BoolParam* multithreading_ = nullptr;

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(TurntableCameraExample, "vsgturntablecamera", "app",
                        "vsgviewer's skeleton with the community Turntable camera controller instead of Trackball: "
                        "WASDQE attitude keys, arrow keys to strafe, IO to fly, mouse drag/wheel, with throw. "
                        "Click into the viewport and hold W/S to feel the difference from the trackball.");

}  // namespace vsglab::examples
