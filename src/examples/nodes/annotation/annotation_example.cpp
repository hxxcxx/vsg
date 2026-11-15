/**
 * @file annotation_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgannotation（派生自 vsgviewer）
 *
 * VK_EXT_debug_utils 调试标签：Annotation<T> 模板把节点包一层，录制时在命令流里
 * 发射命名标签区间——RenderDoc 等工具的命令流浏览器里每个模型都带名字和颜色。
 * 附带一个 Inherit 的坑位演示（NamedGroup）：自定义 accept(RecordTraversal&) 后，
 * 再派生时必须重新暴露它——Inherit::accept 调的 apply() 不是虚函数，
 * 会分派到基类 Group 的录制路径，丢掉注解。
 *
 * 与原版的差异：
 * - 原版从命令行接收多个模型文件并沿 X 轴排开；试验场用两个文件参数
 *   （第二个可清空跳过），布局逻辑（1.1 倍宽间距）不变。
 * - 去掉了 -t / --st / -f / --fps（基准；平均帧率改在 stop() 打印）、
 *   -p（相机动画）、--load-levels / --maxPagedLOD / --hmh（PagedLOD 支线）、
 *   --rgb、--log-level。
 * - --no-annotate 改为界面参数 "Enable debug utils"（默认开）；
 *   --sdi 着色器调试信息改为参数 "Shader debug info"。
 *
 * @author hxxcxx
 * @date 2026-11-15
 */

#include "examples/nodes/annotation/annotation.h"
#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::DoubleParam;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

using NamedTransform = experimental::Annotation<vsg::MatrixTransform>;

// Create a custom subclass with a different display color. This illustrates a gotcha with
// vsg::Inherit and a custom accept(RecordTraversal&): Inherit::accept() calls
// RecordTraversal::apply(), but that is not virtual and will end up doing the recording action for
// vsg::Group, which would leave us without an annotation.
class NamedGroup : public vsg::Inherit<experimental::Annotation<vsg::Group>, NamedGroup> {
public:
    NamedGroup() { debugColor = vsg::vec4(.8f, .8f, .8f, 1.0f); }

    using experimental::Annotation<vsg::Group>::accept;

    void accept(vsg::RecordTraversal& visitor) const override { experimental::Annotation<vsg::Group>::accept(visitor); }
};

// Lay out the loaded files on the X axis
vsg::ref_ptr<NamedGroup> createAnnotatedScene(const std::vector<std::string>& names,
                                              const std::vector<vsg::ref_ptr<vsg::Node>>& nodes) {
    auto scene = NamedGroup::create();
    scene->annotation = "Scene";
    double xExtent = 0.0;
    auto nameItr = names.begin();
    auto nodeItr = nodes.begin();
    for (; nodeItr != nodes.end(); ++nameItr, ++nodeItr) {
        vsg::ComputeBounds computeBounds;
        (*nodeItr)->accept(computeBounds);
        double width = (computeBounds.bounds.max.x - computeBounds.bounds.min.x) * 1.1;
        vsg::dmat4 mat = vsg::translate(xExtent + width / 2, 0.0, 0.0);
        auto matNode = NamedTransform::create(mat);
        matNode->annotation = *nameItr;
        matNode->addChild(*nodeItr);
        scene->addChild(matNode);
        xExtent += width;
    }
    return scene;
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

class AnnotationExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_1_ = params.addFile("Model 1", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                  { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        model_2_ = params.addFile("Model 2", "models/teapot.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                  { "models/teapot.vsgt", "models/lz.vsgt" }, Apply::Restart);
        debug_utils_ = params.addBool("Enable debug utils", true, Apply::Restart);
        shader_debug_info_ = params.addBool("Shader debug info (--sdi)", false, Apply::Restart);
        near_far_ratio_ = params.addDouble("Near/far ratio (--nfr)", 0.001, 1e-6, 0.1, 0.001, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();

        // Use the VK_EXT_debug_utils extension, the point of this demo
        windowTraits->debugUtils = debug_utils_->value();

        auto options = vsg::Options::create(*ctx.options());

        if (shader_debug_info_->value()) {
            enableGenerateDebugInfo(options);
            windowTraits->deviceExtensionNames.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        }

        std::vector<std::string> fileNames;
        std::vector<vsg::ref_ptr<vsg::Node>> nodes;
        for (FileParam* param : { model_1_, model_2_ }) {
            const vsg::Path filename = param->value();
            if (filename.empty())
                continue;

            vsg::Path path = vsg::findFile(filename, options);

            auto object = vsg::read(filename, options);
            if (auto node = object.cast<vsg::Node>()) {
                fileNames.push_back(path.string());
                nodes.push_back(node);
            } else if (object) {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
            } else {
                std::cout << "Unable to load file " << filename << std::endl;
            }
        }

        if (nodes.empty())
            throw vsg::Exception{ "Please specify a 3d model or image file." };

        auto vsg_scene = createAnnotatedScene(fileNames, nodes);

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

        // set up the camera
        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        const double aspectRatio =
                static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
        if (ellipsoidModel) {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, aspectRatio,
                                                            near_far_ratio_->value(), 0.0);
        } else {
            perspective = vsg::Perspective::create(30.0, aspectRatio, near_far_ratio_->value() * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
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
    FileParam* model_1_ = nullptr;
    FileParam* model_2_ = nullptr;
    BoolParam* debug_utils_ = nullptr;
    BoolParam* shader_debug_info_ = nullptr;
    DoubleParam* near_far_ratio_ = nullptr;

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(AnnotationExample, "vsgannotation", "nodes",
                        "VK_EXT_debug_utils labels: wrap nodes with Annotation<T> to emit named, colored "
                        "regions into the Vulkan command stream that show up in RenderDoc's event browser. "
                        "Also demonstrates the Inherit gotcha when overriding accept(RecordTraversal&).");

}  // namespace vsglab::examples
