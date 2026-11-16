/**
 * @file paged_lod_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgpagedlod
 *
 * vsg::PagedLOD 的"手搓版"：自定义 TileReader（ReaderWriter）自己搭瓦片金字塔——
 * 读 "root.tile" 从根瓦片起步，每个瓦片是一个 PagedLOD，子瓦片按需分页加载/卸载。
 * 与 vsgtiledatabase（现成的 TileDatabase）对照着看，正好是"引擎盖下"的完整实现。
 *
 * 与原版的差异：
 * - 瓦片源固定为 Bing 直连（https://ecn.t3.tiles.virtualearth.net/tiles/h{quadkey}.jpeg，
 *   免 key）：本机网络下 OSM / ReadyMap 不可达；TileReader 新增了 {quadkey} 模板支持。
 * - 去掉了 --osm / --rm 瓦片源选择、--poi（数字键观景点已覆盖）、-p（相机动画）、
 *   -o（写场景）、-f、--rgb、--file-cache、--ot（操作线程数，恒为 0）、--log-level。
 * - 原版在循环结束后打印瓦片读取统计，这里改在 stop() 打印。
 *
 * @author hxxcxx
 * @date 2026-11-16
 */

#include "examples/nodes/pagedlod/tile_reader.h"
#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <mutex>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::DoubleParam;
using framework::IntParam;
using framework::ParamSet;
using framework::WindowContext;

class PagedLodExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        max_level_ = params.addInt("Max level (-m)", 19, 1, 21, Apply::Restart);
        lod_transition_ = params.addDouble("LOD transition ratio (-t)", 0.25, 0.01, 1.0, 0.01, Apply::Restart);
        load_levels_ = params.addInt("Preload levels (--load-levels)", 0, 0, 20, Apply::Restart);
        max_paged_lod_ = params.addInt("Max high-res PagedLOD (--maxPagedLOD)", 0, 0, 10000, Apply::Restart);
        horizon_ = params.addDouble("Horizon mountain height (--hmh)", 0.0, 0.0, 10000.0, 10.0, Apply::Restart);
        ellipsoid_perspective_ = params.addBool("Ellipsoid perspective", true, Apply::Restart);
        osgearth_mouse_ = params.addBool("osgEarth mouse buttons (-e)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();

        tile_reader_ = pagedlod::TileReader::create();

        auto options = vsg::Options::create(*ctx.options());
        options->add(tile_reader_);

        // Bing 直连：根层 2×2 = Bing 第 1 级，quadkey 由 TileReader 的 {quadkey} 模板生成
        tile_reader_->noX = 2;
        tile_reader_->noY = 2;
        tile_reader_->maxLevel = static_cast<uint32_t>(max_level_->value());
        tile_reader_->originTopLeft = true;
        tile_reader_->projection = "EPSG:3857";  // spherical-mercator
        tile_reader_->lodTransitionScreenHeightRatio = lod_transition_->value();
        tile_reader_->imageLayer = "https://ecn.t3.tiles.virtualearth.net/tiles/h{quadkey}.jpeg?g=1236";

        tile_reader_->init();

        auto vsg_scene = vsg::read_cast<vsg::Node>("root.tile", options);
        if (!vsg_scene)
            throw vsg::Exception{ "Failed to read root.tile" };

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.0005;

        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");

        vsg::ref_ptr<vsg::LookAt> lookAt;
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (ellipsoidModel) {
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre,
                                         vsg::dvec3(0.0, 0.0, 1.0));
            if (ellipsoid_perspective_->value()) {
                perspective = vsg::EllipsoidPerspective::create(
                        lookAt, ellipsoidModel, 30.0,
                        static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                        nearFarRatio, horizon_->value());
            } else {
                perspective = vsg::Perspective::create(
                        30.0,
                        static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                        nearFarRatio * radius, radius * 4.5);
            }
        } else {
            perspective = vsg::Perspective::create(
                    30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    nearFarRatio * radius, radius * 4.5);
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre,
                                         vsg::dvec3(0.0, 0.0, 1.0));
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        if (ellipsoidModel) {
            auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
            trackball->addKeyViewpoint(vsg::KeySymbol('1'), 51.50151088842245, -0.14181489107549874, 2000.0,
                                       2.0);  // Greenwich Observatory
            trackball->addKeyViewpoint(vsg::KeySymbol('2'), 55.948642740309324, -3.199226855522667, 2000.0,
                                       2.0);  // Edinburgh Castle
            trackball->addKeyViewpoint(vsg::KeySymbol('3'), 48.858264952330764, 2.2945039609604665, 2000.0,
                                       2.0);  // Eiffel Town, Paris
            trackball->addKeyViewpoint(vsg::KeySymbol('4'), 52.5162603714634, 13.377684902745642, 2000.0,
                                       2.0);  // Brandenburg Gate, Berlin
            trackball->addKeyViewpoint(vsg::KeySymbol('5'), 30.047448591298807, 31.236319571791213, 10000.0,
                                       2.0);  // Cairo
            trackball->addKeyViewpoint(vsg::KeySymbol('6'), 35.653099536061156, 139.74704060056993, 10000.0,
                                       2.0);  // Tokyo
            trackball->addKeyViewpoint(vsg::KeySymbol('7'), 37.38701052699002, -122.08555895549424, 10000.0,
                                       2.0);  // Mountain View, California
            trackball->addKeyViewpoint(vsg::KeySymbol('8'), 40.689618207006355, -74.04465595488215, 10000.0,
                                       2.0);  // Empire State Building
            trackball->addKeyViewpoint(vsg::KeySymbol('9'), 25.997055873649554, -97.15543476551771, 1000.0,
                                       2.0);  // Boca Chica, Texas

            if (osgearth_mouse_->value()) {
                trackball->panButtonMask = vsg::BUTTON_MASK_1;
                trackball->rotateButtonMask = vsg::BUTTON_MASK_2;
                trackball->zoomButtonMask = vsg::BUTTON_MASK_3;
            }

            viewer->addEventHandler(trackball);
        } else {
            viewer->addEventHandler(vsg::Trackball::create(camera));
        }

        if (load_levels_->value() > 0) {
            vsg::LoadPagedLOD loadPagedLOD(camera, load_levels_->value());
            auto startTime = std::chrono::steady_clock::now();
            vsg_scene->accept(loadPagedLOD);
            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(
                                std::chrono::steady_clock::now() - startTime)
                                .count();
            std::cout << "No. of tiles loaed " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        if (max_paged_lod_->value() > 0) {
            for (auto& task : viewer->recordAndSubmitTasks) {
                if (task->databasePager)
                    task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs =
                            static_cast<uint32_t>(max_paged_lod_->value());
            }
        }

        ctx.viewer = viewer;
    }

    void stop(WindowContext& /*ctx*/) override {
        // 原版在渲染循环结束后打印：分页器的读取统计
        if (!tile_reader_)
            return;
        std::scoped_lock<std::mutex> lock(tile_reader_->statsMutex);
        std::cout << "numTilesRead = " << tile_reader_->numTilesRead << std::endl;
        std::cout << "average TimeReadingTiles = "
                  << (tile_reader_->totalTimeReadingTiles / static_cast<double>(tile_reader_->numTilesRead))
                  << std::endl;
    }

private:
    IntParam* max_level_ = nullptr;
    DoubleParam* lod_transition_ = nullptr;
    IntParam* load_levels_ = nullptr;
    IntParam* max_paged_lod_ = nullptr;
    DoubleParam* horizon_ = nullptr;
    BoolParam* ellipsoid_perspective_ = nullptr;
    BoolParam* osgearth_mouse_ = nullptr;

    vsg::ref_ptr<pagedlod::TileReader> tile_reader_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(PagedLodExample, "vsgpagedlod", "nodes",
                        "PagedLOD from scratch: a custom TileReader ReaderWriter builds the tile pyramid itself "
                        "and pages subtiles via PagedLOD — the open-hood counterpart of vsgtiledatabase. Bing "
                        "direct-access tiles via the new {quadkey} template; stop prints tile read statistics.");

}  // namespace vsglab::examples
