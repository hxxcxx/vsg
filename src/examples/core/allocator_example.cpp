/**
 * @file allocator_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgallocator
 *
 * 加载模型并渲染，在 viewer 释放前后打印 vsg::Allocator 的内存使用报告，
 * 演示 VSG 内存分配器的统计、空内存块回收（deleteEmptyMemoryBlocks）与分配器校验。
 *
 * 与原版的差异：
 * - 去掉了 --std / --alignment / --allocator / --objects / --nodes / --data：
 *   它们替换整个进程的 vsg::Allocator::instance()，只能在程序启动、分配任何 vsg 对象之前设置，
 *   无法在多实例常驻的试验场中按实例切换。
 * - 去掉了结尾的 vsg::ObjectFactory::instance() = {}：它会清空全局对象工厂，之后整个程序都无法再读取文件。
 * - 去掉了 -t / --st（全屏/小窗口测试）、-f（帧数）、--no-viewer、--cpu（会设置界面线程的 CPU 亲和性）。
 * - 原版在退出渲染循环后打印报告，这里改为在示例停止（stop()）时打印。
 * - 分配器是进程级的，报告中的数字包含其他实例及程序本身的 vsg 对象。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <map>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::DoubleParam;
using framework::ExampleContext;
using framework::ExampleInfo;
using framework::ExampleKind;
using framework::FileParam;
using framework::IntParam;
using framework::ParamSet;

struct SceneStatistics : public vsg::Inherit<vsg::ConstVisitor, SceneStatistics> {
    std::map<const char*, size_t> objectCounts;

    void report(std::ostream& out) {
        for (auto& [str, count] : objectCounts)
            out << "  " << str << " " << count << std::endl;
    }

    void apply(const vsg::Node& node) override {
        ++objectCounts[node.className()];
        node.traverse(*this);
    }
};

class AllocatorExample : public framework::Example {
public:
    ExampleInfo info() const override {
        return { "vsgallocator", "core",
                 "Load and render a model, then report vsg::Allocator memory usage before and after the viewer is "
                 "released. The original's allocator switching options are not available here because they replace "
                 "the process-wide allocator; reports cover the whole process, including other instances.",
                 ExampleKind::Window };
    }

    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        num_stats_ = params.addInt("Stats traversals (--num-stats)", 0, 0, 1000, Apply::Restart);
        shared_objects_ = params.addBool("Shared objects (--so)", false, Apply::Restart);
        keep_rgb_ = params.addBool("Keep RGB images (--rgb)", false, Apply::Restart);
        load_levels_ = params.addInt("Preload PagedLOD levels (--load-levels)", 0, 0, 30, Apply::Restart);
        max_paged_lod_ = params.addInt("Max high-res PagedLOD (--maxPagedLOD)", 0, 0, 100000, Apply::Restart);
        horizon_mountain_height_ =
                params.addDouble("Horizon mountain height (--hmh)", 0.0, 0.0, 10000.0, 10.0, Apply::Restart);
        camera_path_ = params.addFile("Camera animation (-p)", "", "Animations (*.vsgt *.vsgb)", {}, Apply::Restart);
        validate_ = params.addBool("Validate allocator every frame (--validate)", false, Apply::Live);
    }

    void start(ExampleContext& ctx) override {
        intrusive_allocator_ = dynamic_cast<vsg::IntrusiveAllocator*>(vsg::Allocator::instance().get());

        auto windowTraits = ctx.windowTraits();

        // 复制宿主的读取选项，本实例的设置（如 sharedObjects）不影响之后的运行
        auto options = vsg::Options::create(*ctx.options());

        if (shared_objects_->value()) {
            options->sharedObjects = vsg::SharedObjects::create();
            std::cout << "Assigned vsg::SharedObjects " << options->sharedObjects << std::endl;
        }
        if (keep_rgb_->value())
            options->mapRGBtoRGBAHint = false;

        const vsg::Path pathFilename = camera_path_->value();
        const int loadLevels = load_levels_->value();
        const double horizonMountainHeight = horizon_mountain_height_->value();
        const int maxPagedLOD = max_paged_lod_->value();
        const size_t stats = static_cast<size_t>(num_stats_->value());

        // record time point just before loading the scene graph
        auto startOfLoad = vsg::clock::now();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "Unable to load file " + model_->value() };

        // record the total time taken loading the scene graph
        load_duration_ =
                std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - startOfLoad)
                        .count();

        if (stats > 0) {
            auto startOfStats = vsg::clock::now();

            auto sceneStatistics = SceneStatistics::create();

            for (size_t i = 0; i < stats; ++i) {
                sceneStatistics->objectCounts.clear();
                vsg_scene->accept(*sceneStatistics);
            }

            auto statsDuration =
                    std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - startOfStats)
                            .count();

            std::cout << "Stats collection took " << statsDuration << "ms"
                      << " for " << stats << " traversals." << std::endl;
            sceneStatistics->report(std::cout);
        }

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

        const double aspectRatio =
                static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel) {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, aspectRatio, nearFarRatio,
                                                            horizonMountainHeight);
        } else {
            perspective = vsg::Perspective::create(30.0, aspectRatio, nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        if (pathFilename) {
            auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
            viewer->addEventHandler(cameraAnimation);
            if (cameraAnimation->animation)
                cameraAnimation->play();
        }

        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        // if required preload specific number of PagedLOD levels.
        if (loadLevels > 0) {
            vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

            auto startTime = std::chrono::steady_clock::now();

            vsg_scene->accept(loadPagedLOD);

            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(
                                std::chrono::steady_clock::now() - startTime)
                                .count();
            std::cout << "No. of tiles loaded " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        if (maxPagedLOD > 0) {
            // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager
            // if required.
            for (auto& task : viewer->recordAndSubmitTasks) {
                if (task->databasePager)
                    task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
            }
        }

        start_of_frame_loop_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void frame(ExampleContext& ctx) override {
        Example::frame(ctx);

        if (validate_->value() && intrusive_allocator_) {
            if (!intrusive_allocator_->validate()) {
                vsg::warn("intrusiveAllocator->validate() failed test, frameCount = ",
                          ctx.viewer->getFrameStamp()->frameCount);
            }
        }
    }

    void stop(ExampleContext& ctx) override {
        // start() 失败时没有 viewer，也就没有统计可报告
        if (!ctx.viewer)
            return;

        double frameRate = 0.0;
        if (auto frameStamp = ctx.viewer->getFrameStamp(); frameStamp && frameStamp->frameCount > 0) {
            auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() -
                                                                                        start_of_frame_loop_)
                                    .count();
            frameRate = (double(frameStamp->frameCount) / duration);
        }

        std::cout << "\nBefore end of Viewer scope." << std::endl;
        vsg::Allocator::instance()->report(std::cout);

        // record the end of viewer scope
        auto endOfViewerScope = vsg::clock::now();

        // 对应原版离开 viewer 作用域：释放 viewer 及其持有的窗口、相机与场景
        ctx.viewer->deviceWaitIdle();
        ctx.viewer = nullptr;

        double releaseDuration =
                std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - endOfViewerScope)
                        .count();

        std::cout << "\nAfter end of Viewer scope." << std::endl;
        vsg::Allocator::instance()->report(std::cout);

        // Optional call to delete any empty memory blocks, this won't normally be required in a VSG application, but
        // if your memory usage goes up and down regularly and you want to free up memory for use elsewhere then you
        // can call vsg::Allocator::instance()->deleteEmptyMemoryBlocks() after you delete scene graph nodes, data and
        // objects to make sure any memory blocks that may now be empty can be released back to the OS.
        auto beforeDelete = vsg::clock::now();
        auto memoryDeleted = vsg::Allocator::instance()->deleteEmptyMemoryBlocks();
        double deleteDuration =
                std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - beforeDelete)
                        .count();

        std::cout << "\nAfter delete of empty memory blocks, where " << memoryDeleted << " was freed." << std::endl;
        vsg::Allocator::instance()->report(std::cout);

        std::cout << "\nload duration = " << load_duration_ << "ms" << std::endl;
        std::cout << "release duration  = " << releaseDuration << "ms" << std::endl;
        std::cout << "delete duration  = " << deleteDuration << "ms" << std::endl;
        std::cout << "Average frame rate = " << frameRate << "fps" << std::endl;
    }

private:
    FileParam* model_ = nullptr;
    IntParam* num_stats_ = nullptr;
    BoolParam* shared_objects_ = nullptr;
    BoolParam* keep_rgb_ = nullptr;
    IntParam* load_levels_ = nullptr;
    IntParam* max_paged_lod_ = nullptr;
    DoubleParam* horizon_mountain_height_ = nullptr;
    FileParam* camera_path_ = nullptr;
    BoolParam* validate_ = nullptr;

    vsg::IntrusiveAllocator* intrusive_allocator_ = nullptr;
    double load_duration_ = 0.0;
    vsg::time_point start_of_frame_loop_;
};

}  // namespace

std::unique_ptr<framework::Example> makeAllocatorExample() {
    return std::make_unique<AllocatorExample>();
}

}  // namespace vsglab::examples
