/**
 * @file viewer_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgviewer（vsgExamples 的"旗舰查看器"）
 *
 * vsghelloworld 的全参数版：支持多模型/图片（图片自动贴成 Quad）、
 * 椭球感知的相机初始化（小于地球的模型按"落地面"取景，大于的按全球取景）、
 * 相机动画路径回放（-p）、头灯、PagedLOD 预加载、LODScale 实时调节、
 * GPU 标注（RenderDoc 注解）、多线程录制。
 *
 * 与原版的差异：
 * - 模型由两个文件参数提供（原版命令行任意多个；第二个可清空跳过）。
 * - 去掉了 -t/--st/--fps/--rms（基准与统计；平均帧率改在 stop() 打印）、
 *   -c CPU 亲和性、--log-level/--log/--redirect-std/--ThreadLogger（宿主已有日志桥）、
 *   --ss 外部 ShaderSet、--rh/--orh、--ot、--rgb、--max-time、--profiler。
 * - --ga GPU 标注、--sdi 着色器调试信息、--dc 深度钳制、--mt 多线程、
 *   --no-auto-play 均改为界面参数；LODScale 为 Live（实时生效）。
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
using framework::BoolParam;
using framework::DoubleParam;
using framework::FileParam;
using framework::IntParam;
using framework::ParamBase;
using framework::ParamSet;
using framework::WindowContext;

vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData, vsg::ref_ptr<vsg::Options> options) {
    auto builder = vsg::Builder::create();
    builder->options = options;
    vsg::StateInfo state;
    state.image = sourceData;
    state.lighting = false;
    vsg::GeometryInfo geom;
    geom.dx.set(static_cast<float>(sourceData->width()), 0.0f, 0.0f);
    geom.dy.set(0.0f, 0.0f, static_cast<float>(sourceData->height()));
    geom.dz.set(0.0f, -1.0f, 0.0f);
    return builder->createQuad(geom, state);
}

void enableGenerateDebugInfo(vsg::ref_ptr<vsg::Options> options) {
    auto shaderHints = vsg::ShaderCompileSettings::create();
    shaderHints->generateDebugInfo = true;

    auto& text = options->shaderSets["text"] = vsg::createTextShaderSet(options);
    text->defaultShaderHints = shaderHints;

    auto& flat = options->shaderSets["flat"] = vsg::createFlatShadedShaderSet(options);
    flat->defaultShaderHints = shaderHints;

    auto& phong = options->shaderSets["phong"] = vsg::createPhongShaderSet(options);
    phong->defaultShaderHints = shaderHints;

    auto& pbr = options->shaderSets["pbr"] = vsg::createPhysicsBasedRenderingShaderSet(options);
    pbr->defaultShaderHints = shaderHints;
}

class ViewerExample : public framework::WindowExample {
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
        lod_scale_ = params.addDouble("LOD scale (--LODScale)", 1.0, 0.1, 4.0, 0.1, Apply::Live);
        near_far_ratio_ = params.addDouble("Near/far ratio (--nfr)", 0.001, 1e-6, 0.1, 0.001, Apply::Restart);
        horizon_ = params.addDouble("Horizon mountain height (--hmh)", 0.0, 0.0, 10000.0, 10.0, Apply::Restart);
        depth_clamp_ = params.addBool("Depth clamp (--dc)", false, Apply::Restart);
        shader_debug_ = params.addBool("Shader debug info (--sdi)", false, Apply::Restart);
        gpu_annotation_ = params.addBool("GPU annotation (--ga)", false, Apply::Restart);
        multithreading_ = params.addBool("Multi-threaded record (--mt)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = vsg::Options::create(*ctx.options());

        if (depth_clamp_->value()) {
            std::cout << "Enabled depth clamp." << std::endl;
            auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
            deviceFeatures->get().samplerAnisotropy = VK_TRUE;
            deviceFeatures->get().depthClamp = VK_TRUE;
        }

        vsg::ref_ptr<vsg::Instrumentation> instrumentation;
        if (gpu_annotation_->value() && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            windowTraits->debugUtils = true;
            instrumentation = vsg::GpuAnnotation::create();
        }

        if (shader_debug_->value()) {
            enableGenerateDebugInfo(options);
            windowTraits->deviceExtensionNames.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
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

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        // 椭球感知的相机初始化：小于地球的模型按"落到地面"取景，否则按全球取景
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (ellipsoidModel) {
            vsg::ComputeBounds computeBounds;
            vsg_scene->accept(computeBounds);
            double initialRadius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
            double modelToEarthRatio = (initialRadius / ellipsoidModel->radiusEquator());
            if (modelToEarthRatio < 1.0) {
                vsg::dvec3 lla = ellipsoidModel->convertECEFToLatLongAltitude(
                        (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5);
                auto worldToLocal = ellipsoidModel->computeWorldToLocalTransform(lla);
                auto localToWorld = ellipsoidModel->computeLocalToWorldTransform(lla);
                computeBounds.matrixStack.clear();
                computeBounds.matrixStack.push_back(worldToLocal);
                computeBounds.bounds.reset();
                vsg_scene->accept(computeBounds);
                auto bounds = computeBounds.bounds;
                vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
                double radius = vsg::length(bounds.max - bounds.min) * 0.5;
                lookAt = vsg::LookAt::create(localToWorld * (centre + vsg::dvec3(0.0, 0.0, radius)),
                                             localToWorld * centre, vsg::dvec3(0.0, 1.0, 0.0) * worldToLocal);
            } else {
                lookAt = vsg::LookAt::create(vsg::dvec3(initialRadius * 2.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0),
                                             vsg::dvec3(0.0, 0.0, 1.0));
            }
            perspective = vsg::EllipsoidPerspective::create(
                    lookAt, ellipsoidModel, 30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    near_far_ratio_->value(), horizon_->value());
        } else {
            vsg::ComputeBounds computeBounds;
            vsg_scene->accept(computeBounds);
            vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre,
                                         vsg::dvec3(0.0, 0.0, 1.0));
            perspective = vsg::Perspective::create(
                    30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    near_far_ratio_->value() * radius, radius * 10.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, vsg::Path(camera_path_->value()), options);
        viewer->addEventHandler(cameraAnimation);
        if (auto_play_->value() && cameraAnimation->animation) {
            cameraAnimation->play();
        }

        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        if (load_levels_->value() > 0) {
            vsg::LoadPagedLOD loadPagedLOD(camera, load_levels_->value());
            auto startTime = vsg::clock::now();
            vsg_scene->accept(loadPagedLOD);
            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(vsg::clock::now() - startTime)
                                .count();
            std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto view = vsg::View::create(camera);
        view->LODScale = lod_scale_->value();
        view->addChild(vsg::createHeadlight());
        view->addChild(vsg_scene);

        auto renderGraph = vsg::RenderGraph::create(window, view);
        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        if (instrumentation)
            viewer->assignInstrumentation(instrumentation);

        if (multithreading_->value())
            viewer->setupThreading();

        viewer->compile(vsg::ResourceHints::create());

        if (max_paged_lod_->value() > 0) {
            for (auto& task : viewer->recordAndSubmitTasks) {
                if (task->databasePager)
                    task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs =
                            static_cast<uint32_t>(max_paged_lod_->value());
            }
        }

        view_ = view;
        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void onParamChanged(WindowContext& /*ctx*/, const ParamBase& param) override {
        if (&param == lod_scale_ && view_)
            view_->LODScale = lod_scale_->value();
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
    DoubleParam* lod_scale_ = nullptr;
    DoubleParam* near_far_ratio_ = nullptr;
    DoubleParam* horizon_ = nullptr;
    BoolParam* depth_clamp_ = nullptr;
    BoolParam* shader_debug_ = nullptr;
    BoolParam* gpu_annotation_ = nullptr;
    BoolParam* multithreading_ = nullptr;

    vsg::ref_ptr<vsg::View> view_;
    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(ViewerExample, "vsgviewer", "app",
                        "The flagship viewer: multiple models or images (auto-quadded), ellipsoid-aware camera "
                        "framing (small models land on the ground, planet-scale orbit globally), camera animation "
                        "playback, headlight, PagedLOD preloading, live LOD scale, GPU annotation and multi-threaded "
                        "record.");

}  // namespace vsglab::examples
