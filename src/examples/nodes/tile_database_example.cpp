/**
 * @file tile_database_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgtiledatabase
 *
 * vsg::TileDatabase：瓦片金字塔地球——PagedLOD 按相机位置分页加载/卸载瓦片，
 * Trackball 的数字键 1..9 平滑飞到预设观景点（格林尼治/巴黎/东京……）。
 * EllipsoidPerspective 配合 EllipsoidModel 处理地平线尺度的投影。
 *
 * 与原版的差异：
 * - 瓦片源固定为 Bing 直连（createBingMapsSettings 空 key 时自动回退到
 *   https://ecn.t3.tiles.virtualearth.net/tiles/h{quadkey}.jpeg，免 key）：
 *   本机网络下 OSM / ReadyMap / ArcGIS 均不可达（实测超时或 403），唯 Bing 通。
 *   原版的 --bing-maps/--rm/--rme/--osm 四选一与 --detail 支线随之移除。
 * - 去掉了 --poi（数字键观景点已覆盖）、-p（相机动画路径）、-o（写场景）、
 *   -f、--rgb、--file-cache、--ot、--mtd、--log-level。
 * - --bing-maps 的 imagerySet/culture/key 未做成参数（直连端点不使用它们）。
 *
 * @author hxxcxx
 * @date 2026-11-16
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
using framework::ColorParam;
using framework::DoubleParam;
using framework::IntParam;
using framework::ParamSet;
using framework::WindowContext;

class TileDatabaseExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        max_level_ = params.addInt("Max level (-m)", 19, 1, 22, Apply::Restart);
        lod_transition_ = params.addDouble("LOD transition ratio (-t)", 0.25, 0.01, 1.0, 0.01, Apply::Restart);
        load_levels_ = params.addInt("Preload levels (--load-levels)", 0, 0, 20, Apply::Restart);
        max_paged_lod_ = params.addInt("Max high-res PagedLOD (--maxPagedLOD)", 0, 0, 10000, Apply::Restart);
        horizon_ = params.addDouble("Horizon mountain height (--hmh)", 0.0, 0.0, 10000.0, 10.0, Apply::Restart);
        ellipsoid_perspective_ = params.addBool("Ellipsoid perspective", true, Apply::Restart);
        osgearth_mouse_ = params.addBool("osgEarth mouse buttons (-e)", false, Apply::Restart);
        clear_color_ = params.addColor("Clear color (--clear)", vsg::vec4(0.2f, 0.2f, 0.4f, 1.0f), Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        // Bing 直连：key 留空时 createBingMapsSettings 自动回退到免 key 的 quadkey 端点
        auto settings = vsg::createBingMapsSettings("Aerial", "en-GB", "", options);
        settings->maxLevel = static_cast<uint32_t>(max_level_->value());
        settings->lodTransitionScreenHeightRatio = lod_transition_->value();

        auto ellipsoidModel = settings->ellipsoidModel;

        auto earth = vsg::TileDatabase::create();
        earth->settings = settings;
        earth->readDatabase(options);

        auto vsg_scene = vsg::ref_ptr<vsg::Node>(earth);

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
        double nearFarRatio = 0.0005;

        // set up the camera
        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
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

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        // 数字键观景点：配合航拍图环游世界
        auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
        trackball->addKeyViewpoint(vsg::KeySymbol('1'), 51.50151088842245, -0.14181489107549874, 2000.0,
                                   2.0);  // Greenwich Observatory
        trackball->addKeyViewpoint(vsg::KeySymbol('2'), 55.948642740309324, -3.199226855522667, 2000.0,
                                   2.0);  // Edinburgh Castle
        trackball->addKeyViewpoint(vsg::KeySymbol('3'), 48.858264952330764, 2.2945039609604665, 2000.0,
                                   2.0);  // Eiffel Town, Paris
        trackball->addKeyViewpoint(vsg::KeySymbol('4'), 52.5162603714634, 13.377684902745642, 2000.0,
                                   2.0);  // Brandenburg Gate, Berlin
        trackball->addKeyViewpoint(vsg::KeySymbol('5'), 30.047448591298807, 31.236319571791213, 10000.0, 2.0);  // Cairo
        trackball->addKeyViewpoint(vsg::KeySymbol('6'), 35.653099536061156, 139.74704060056993, 10000.0, 2.0);  // Tokyo
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

        // if required pre load specific number of PagedLOD levels.
        if (load_levels_->value() > 0) {
            vsg::LoadPagedLOD loadPagedLOD(camera, load_levels_->value());

            auto startTime = std::chrono::steady_clock::now();

            vsg_scene->accept(loadPagedLOD);

            auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(
                                std::chrono::steady_clock::now() - startTime)
                                .count();
            std::cout << "No. of tiles loaed " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
        }

        auto rendergraph = vsg::createRenderGraphForView(window, camera, vsg_scene);
        rendergraph->setClearValues(vsg::sRGB_to_linear(clear_color_->value()));

        auto commandGraph = vsg::CommandGraph::create(window, rendergraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        if (max_paged_lod_->value() > 0) {
            // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
            for (auto& task : viewer->recordAndSubmitTasks) {
                if (task->databasePager)
                    task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs =
                            static_cast<uint32_t>(max_paged_lod_->value());
            }
        }

        ctx.viewer = viewer;
    }

private:
    IntParam* max_level_ = nullptr;
    DoubleParam* lod_transition_ = nullptr;
    IntParam* load_levels_ = nullptr;
    IntParam* max_paged_lod_ = nullptr;
    DoubleParam* horizon_ = nullptr;
    BoolParam* ellipsoid_perspective_ = nullptr;
    BoolParam* osgearth_mouse_ = nullptr;
    ColorParam* clear_color_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(TileDatabaseExample, "vsgtiledatabase", "nodes",
                        "vsg::TileDatabase: a tile-pyramid earth with PagedLOD paging tiles in/out as the camera "
                        "moves. Number keys 1..9 fly to preset viewpoints (Greenwich, Paris, Tokyo...). Bing "
                        "direct-access tiles (no key); OSM/ReadyMap unreachable on this network.");

}  // namespace vsglab::examples
