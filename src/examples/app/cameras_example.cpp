/**
 * @file cameras_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgcameras
 *
 * 场景内相机与多视口监控墙：若场景图里没有相机（FindCameras 遍历查找），
 * 就补挂 Left/Right/Front/Top 四台；主视口交互浏览，右上角为每台场景相机
 * 开一个小视口（TrackingViewMatrix 跟随该相机在场景图中的位姿）。
 * CameraSelector 事件处理器：数字键 1..9 把主相机"瞬移"成对应场景相机的
 * 视角（含节点路径累计变换），0 恢复初始视角，滚轮按键前后切换。
 *
 * 与原版的差异：
 * - 原版缺省加载 models/lz.vsgt（命令行可换）；试验场改为文件参数。
 * - 去掉了 --ga/--pr 检测（留待 utils/vsginstrumentation）、-t/-f/--fps
 *   （基准；平均帧率改在 stop() 打印）、--rh、--log。
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

class CameraSelector : public vsg::Inherit<vsg::Visitor, CameraSelector> {
public:
    using Cameras = decltype(vsg::FindCameras::cameras);

    CameraSelector(vsg::ref_ptr<vsg::Camera> in_camera, const Cameras& in_cameras)
        : main_camera(in_camera), cameras(in_cameras) {
        auto lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
        if (lookAt)
            original_viewMatrix = vsg::LookAt::create(*lookAt);
    }

    vsg::ref_ptr<vsg::Camera> main_camera;
    vsg::ref_ptr<vsg::LookAt> original_viewMatrix;
    Cameras cameras;
    int currentCameraIndex = 0;

    void apply(vsg::ButtonPressEvent& buttonPress) override {
        if (cameras.empty())
            return;

        if (buttonPress.button == 4) {
            // next camera
            currentCameraIndex++;
            currentCameraIndex %= cameras.size();

            selectCameraByIndex(currentCameraIndex);
        } else if (buttonPress.button == 5) {
            // previous camera
            currentCameraIndex--;
            currentCameraIndex %= cameras.size();

            selectCameraByIndex(currentCameraIndex);
        }
    }

    void apply(vsg::KeyPressEvent& keyPress) override {
        if (keyPress.keyBase == '0') {
            if (original_viewMatrix) {
                auto lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
                if (lookAt) {
                    *(lookAt) = *original_viewMatrix;
                }
            }
        } else if ((keyPress.keyBase >= '1') && (keyPress.keyBase <= '9')) {
            currentCameraIndex = keyPress.keyBase - '1';

            selectCameraByIndex(currentCameraIndex);
        }
    }

private:
    void selectCameraByIndex(int index) {
        int cameraIndex = 0;
        for (auto& [nodePath, camera] : cameras) {
            if (cameraIndex == index) {
                auto begin = nodePath.begin();
                auto end = nodePath.end();
                if (begin != end)
                    --end;

                auto matrix = vsg::visit<vsg::ComputeTransform>(begin, end).matrix;

                std::cout << "Matched: " << camera->name << " " << matrix << std::endl;

                auto selected_lookAt = camera->viewMatrix.cast<vsg::LookAt>();
                auto main_lookAt = main_camera->viewMatrix.cast<vsg::LookAt>();
                if (main_lookAt) {
                    *main_lookAt = *selected_lookAt;
                    main_lookAt->transform(matrix);
                }
                break;
            }
            ++cameraIndex;
        }
    }
};

class CamerasExample : public framework::WindowExample {
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

        // compute the bounds of the scene graph to help position camera
        auto bounds = vsg::visit<vsg::ComputeBounds>(scenegraph).bounds;
        vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
        double radius = vsg::length(bounds.max - bounds.min) * 0.5;
        double nearFarRatio = 0.001;

        auto scene_cameras = vsg::visit<vsg::FindCameras>(scenegraph).cameras;

        if (scene_cameras.empty()) {
            // no cameras are present in the scene graph so add them
            auto root = vsg::Group::create();
            root->addChild(scenegraph);

            auto perspective = vsg::Perspective::create(90.0, 1.0, nearFarRatio * radius, radius * 4.0);

            // left
            {
                auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(radius * 2.0, 0.0, 0.0), centre,
                                                  vsg::dvec3(0.0, 0.0, 1.0));
                auto camera = vsg::Camera::create(perspective, lookAt);
                camera->name = "Left";
                root->addChild(camera);
            }

            // right
            {
                auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(-radius * 2.0, 0.0, 0.0), centre,
                                                  vsg::dvec3(0.0, 0.0, 1.0));
                auto camera = vsg::Camera::create(perspective, lookAt);
                camera->name = "Right";
                root->addChild(camera);
            }

            // front
            {
                auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 2.0, 0.0), centre,
                                                  vsg::dvec3(0.0, 0.0, 1.0));
                auto camera = vsg::Camera::create(perspective, lookAt);
                camera->name = "Front";
                root->addChild(camera);
            }

            // top
            {
                auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 2.0), centre,
                                                  vsg::dvec3(0.0, 1.0, 0.0));
                auto camera = vsg::Camera::create(perspective, lookAt);
                camera->name = "Top";
                root->addChild(camera);
            }

            scenegraph = root;

            // refresh the scene_cameras list.
            scene_cameras = vsg::visit<vsg::FindCameras>(scenegraph).cameras;
        }

        for (auto& [nodePath, camera] : scene_cameras) {
            std::cout << "\ncamera = " << camera << ", " << camera->name << " :";
            for (auto& node : nodePath)
                std::cout << " " << node;
            std::cout << std::endl;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        uint32_t width = window->extent2D().width;
        uint32_t height = window->extent2D().height;

        // CommandGraph to hold the different RenderGraphs used to render each view
        auto commandGraph = vsg::CommandGraph::create(window);

        // add headlights to views to make sure any objects that need lighting have it.
        auto headlight = vsg::createHeadlight();

        // set up main interactive view
        {
            auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre,
                                              vsg::dvec3(0.0, 0.0, 1.0));

            auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                        nearFarRatio * radius, radius * 4.5);

            auto viewportState = vsg::ViewportState::create(0, 0, width, height);

            // create the vsg::RenderGraph and associated vsg::View
            auto main_camera = vsg::Camera::create(perspective, lookAt, viewportState);
            auto main_view = vsg::View::create(main_camera, scenegraph);
            main_view->addChild(headlight);

            auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);

            commandGraph->addChild(main_RenderGraph);

            viewer->addEventHandler(vsg::Trackball::create(main_camera));
            viewer->addEventHandler(CameraSelector::create(main_camera, scene_cameras));
        }

        // set up secondary views, one per camera found in the scene graph
        uint32_t margin = 10;
        uint32_t division = static_cast<uint32_t>(scene_cameras.size());
        if (division < 3)
            division = 3;

        uint32_t secondary_width = width / division;
        uint32_t secondary_height = ((height - margin) / division) - margin;

        uint32_t x = width - secondary_width - margin;
        uint32_t y = margin;

        for (auto& [nodePath, camera] : scene_cameras) {
            // create a RenderGraph to add a secondary vsg::View on the top right part of the window.
            auto projectionMatrix = vsg::clone(camera->projectionMatrix);
            auto viewMatrix = vsg::TrackingViewMatrix::create(nodePath);
            auto viewportState = vsg::ViewportState::create(x, y, secondary_width, secondary_height);

            auto secondary_camera = vsg::Camera::create(projectionMatrix, viewMatrix, viewportState);

            auto secondary_view = vsg::View::create(secondary_camera, scenegraph);
            secondary_view->addChild(headlight);

            auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
            secondary_RenderGraph->clearValues[0].color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);
            commandGraph->addChild(secondary_RenderGraph);

            y += secondary_height + margin;
        }

        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

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
                          << " fps" << std::endl;
        }
    }

private:
    FileParam* model_ = nullptr;

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(CamerasExample, "vsgcameras", "app",
                        "Scene-embedded cameras and a monitoring wall: FindCameras locates cameras in the graph "
                        "(adding Left/Right/Front/Top if none), each gets a small tracking viewport top-right "
                        "(TrackingViewMatrix follows its node path). Number keys fly the main camera to a scene "
                        "camera's view, 0 restores. Custom event handler: CameraSelector.");

}  // namespace vsglab::examples
