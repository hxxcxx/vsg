/**
 * @file skybox_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgskybox
 *
 * 天空盒：内嵌 GLSL 的手写管线（不经过 ShaderSet 体系）——24 顶点立方体贴成
 * 全景内壁，CULL_FRONT（裁背面留内壁）、depthWrite 关闭 + GREATER_OR_EQUAL
 * （先画天空后画场景也不会挡住物体）；cubemap 纹理（.dds/.ktx）由 vsgXchange 读取。
 * 天空盒和普通模型装进同一个 Group 照常查看。
 *
 * 与原版的差异：
 * - 天空盒贴图与模型改为文件参数（原版 --skybox + 命令行模型；
 *   默认 textures/skybox.dds，data 里另有 skybox.ktx 可选）。
 * - 去掉了 -t/--st/-f（基准）、-o（写场景）、--hmh 保持默认 0（未做参数）。
 *
 * @author hxxcxx
 * @date 2026-11-17
 */

#include "examples/app/skybox/skybox_shaders.h"
#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <iostream>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

vsg::ref_ptr<vsg::Node> createSkybox(const vsg::Path& filename, vsg::ref_ptr<vsg::Options> options) {
    auto data = vsg::read_cast<vsg::Data>(filename, options);
    if (!data) {
        std::cout << "Error: failed to load cubemap file : " << filename << std::endl;
        return {};
    }

    auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", skybox_vert);
    auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", skybox_frag);

    const vsg::ShaderStages shaders{ vertexShader, fragmentShader };

    vsg::DescriptorSetLayoutBindings descriptorBindings{ { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                           VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        { VK_SHADER_STAGE_VERTEX_BIT, 0, 128 }
        // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{ VkVertexInputBindingDescription{
            0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX } };
    vsg::VertexInputState::Attributes vertexAttributeDescriptions{ VkVertexInputAttributeDescription{
            0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } };

    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_FRONT_BIT;

    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_TRUE;
    depthState->depthWriteEnable = VK_FALSE;
    depthState->depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

    vsg::GraphicsPipelineStates pipelineStates{ vsg::VertexInputState::create(vertexBindingsDescriptions,
                                                                              vertexAttributeDescriptions),
                                                vsg::InputAssemblyState::create(),
                                                rasterState,
                                                vsg::MultisampleState::create(),
                                                vsg::ColorBlendState::create(),
                                                depthState };

    auto pipelineLayout =
            vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout }, pushConstantRanges);
    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaders, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(pipeline);

    auto sampler = vsg::Sampler::create();
    sampler->maxLod = data->properties.mipLevels;
    auto texture = vsg::DescriptorImage::create(sampler, data, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{ texture });
    auto bindDescriptorSet =
            vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    auto root = vsg::StateGroup::create();
    root->add(bindGraphicsPipeline);
    root->add(bindDescriptorSet);

    auto vertices = vsg::vec3Array::create({ // Back
                                             { -1.0f, -1.0f, -1.0f },
                                             { 1.0f, -1.0f, -1.0f },
                                             { -1.0f, 1.0f, -1.0f },
                                             { 1.0f, 1.0f, -1.0f },
                                             // Front
                                             { -1.0f, -1.0f, 1.0f },
                                             { 1.0f, -1.0f, 1.0f },
                                             { -1.0f, 1.0f, 1.0f },
                                             { 1.0f, 1.0f, 1.0f },
                                             // Left
                                             { -1.0f, -1.0f, -1.0f },
                                             { -1.0f, -1.0f, 1.0f },
                                             { -1.0f, 1.0f, -1.0f },
                                             { -1.0f, 1.0f, 1.0f },
                                             // Right
                                             { 1.0f, -1.0f, -1.0f },
                                             { 1.0f, -1.0f, 1.0f },
                                             { 1.0f, 1.0f, -1.0f },
                                             { 1.0f, 1.0f, 1.0f },
                                             // Bottom
                                             { -1.0f, -1.0f, -1.0f },
                                             { -1.0f, -1.0f, 1.0f },
                                             { 1.0f, -1.0f, -1.0f },
                                             { 1.0f, -1.0f, 1.0f },
                                             // Top
                                             { -1.0f, 1.0f, -1.0f },
                                             { -1.0f, 1.0f, 1.0f },
                                             { 1.0f, 1.0f, -1.0f },
                                             { 1.0f, 1.0f, 1.0 } });
    auto indices = vsg::ushortArray::create({ // Back
                                              0, 2, 1, 1, 2, 3,
                                              // Front
                                              6, 4, 5, 7, 6, 5,
                                              // Left
                                              10, 8, 9, 11, 10, 9,
                                              // Right
                                              14, 13, 12, 15, 13, 14,
                                              // Bottom
                                              17, 16, 19, 19, 16, 18,
                                              // Top
                                              23, 20, 21, 22, 20, 23 });
    root->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices }));
    root->addChild(vsg::BindIndexBuffer::create(indices));
    root->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));
    auto xform = vsg::MatrixTransform::create(vsg::rotate(vsg::PI * 0.5, 1.0, 0.0, 0.0));
    xform->addChild(root);
    return xform;
}

class SkyboxExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        skybox_ = params.addFile("Skybox cubemap", "textures/skybox.dds", "Cubemaps (*.dds *.ktx)",
                                 { "textures/skybox.dds", "textures/skybox.ktx" }, Apply::Restart);
        model_ = params.addFile("Model", "models/teapot.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/teapot.vsgt", "models/lz.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto group = vsg::Group::create();

        if (auto node = createSkybox(vsg::Path(skybox_->value()), options)) {
            group->addChild(node);
        } else {
            throw vsg::Exception{ "Failed to create skybox from: " + skybox_->value() };
        }

        if (!model_->value().empty()) {
            auto object = vsg::read(model_->value(), options);
            if (auto node = object.cast<vsg::Node>()) {
                group->addChild(node);
            } else {
                std::cout << "Unable to load model from file " << model_->value() << std::endl;
            }
        }

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

        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        auto lookAt =
                vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel")) {
            perspective = vsg::EllipsoidPerspective::create(
                    lookAt, ellipsoidModel, 30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    nearFarRatio, 0.0);
        } else {
            perspective = vsg::Perspective::create(
                    30.0,
                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                    nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

private:
    FileParam* skybox_ = nullptr;
    FileParam* model_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(SkyboxExample, "vsgskybox", "app",
                        "Skybox with a hand-written pipeline and embedded GLSL: a 24-vertex cube textured from "
                        "inside by a cubemap (CULL_FRONT, no depth write) — the bare-Vulkan way, bypassing the "
                        "ShaderSet system entirely.");

}  // namespace vsglab::examples
