/**
 * @file render_to_texture_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgrendertotexture
 *       （含 Robert Osfield / Tim Moore 及 Sascha Willems 衍生代码的 MIT 许可）
 *
 * 渲染到纹理（RTT）的经典回路：把模型渲进一张离屏纹理（512×512，usage 同时带
 * COLOR_ATTACHMENT 和 SAMPLED——既当靶子又当贴图），再把两个贴这张纹理的
 * 四边形投到主窗口；模型持续自转，两块"电视屏幕"实时同步画面。
 * 管线全部手写（预编译 .spv 着色器 + DescriptorImage 绑定离屏纹理）。
 *
 * 与原版的差异：
 * - 模型改为文件参数（原版命令行模型）。
 * - 去掉了 -n/--nested 与 -s（三种 CommandGraph 拓扑切换，固定用默认单
 *   CommandGraph 路径）、--mt、-f。
 *
 * @author hxxcxx
 * @date 2026-11-19
 */

/* <editor-fold desc="MIT License">
Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore
Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
</editor-fold> */

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

vsg::ref_ptr<vsg::RenderGraph> createOffscreenRendergraph(vsg::Context& context, const VkExtent2D& extent,
                                                          vsg::ImageInfo& colorImageInfo,
                                                          vsg::ImageInfo& depthImageInfo) {
    auto device = context.device;
    VkExtent3D attachmentExtent{ extent.width, extent.height, 1 };

    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = VK_FORMAT_R8G8B8A8_SRGB;
    colorImage->extent = attachmentExtent;
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;  // ★ 既当靶子又当贴图
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto colorImageView = createImageView(context, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);

    auto colorSampler = vsg::Sampler::create();
    colorSampler->flags = 0;
    colorSampler->magFilter = VK_FILTER_LINEAR;
    colorSampler->minFilter = VK_FILTER_LINEAR;
    colorSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    colorSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    colorSampler->addressModeV = colorSampler->addressModeU;
    colorSampler->addressModeW = colorSampler->addressModeU;
    colorSampler->mipLodBias = 0.0f;
    colorSampler->maxAnisotropy = 1.0f;
    colorSampler->minLod = 0.0f;
    colorSampler->maxLod = 1.0f;
    colorSampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    colorImageInfo.imageView = colorImageView;
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // ★ 采样时的布局
    colorImageInfo.sampler = colorSampler;

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = attachmentExtent;
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->flags = 0;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.sampler = nullptr;
    depthImageInfo.imageView = vsg::createImageView(context, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // RenderPass 尾布局直接落在 SHADER_READ_ONLY——渲完即可被采样，无需手动屏障
    vsg::RenderPass::Attachments attachments(2);
    attachments[0].format = VK_FORMAT_R8G8B8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    vsg::AttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    vsg::RenderPass::Subpasses subpassDescription(1);
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].colorAttachments.emplace_back(colorReference);
    subpassDescription[0].depthStencilAttachments.emplace_back(depthReference);

    // 依赖：上一帧的采样 → 本帧的写入；本帧写入 → 下一帧采样（BY_REGION 细粒度）
    vsg::RenderPass::Dependencies dependencies(2);
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto renderPass = vsg::RenderPass::create(device, attachments, subpassDescription, dependencies);
    auto fbuf =
            vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageInfo.imageView, depthImageInfo.imageView },
                                     extent.width, extent.height, 1);
    auto rendergraph = vsg::RenderGraph::create();
    rendergraph->renderArea.offset = VkOffset2D{ 0, 0 };
    rendergraph->renderArea.extent = extent;
    rendergraph->framebuffer = fbuf;
    rendergraph->clearValues.resize(2);
    rendergraph->clearValues[0].color = vsg::sRGB_to_linear(0.4f, 0.2f, 0.4f, 1.0f);
    rendergraph->clearValues[1].depthStencil = VkClearDepthStencilValue{ 0.0f, 0 };
    return rendergraph;
}

vsg::ref_ptr<vsg::Node> createPlanes(vsg::ref_ptr<vsg::ImageInfo> colorImage, vsg::ref_ptr<vsg::Options> options) {
    auto vertexShader =
            vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/vert_PushConstants.spv", options);
    auto fragmentShader =
            vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/frag_PushConstants.spv", options);
    if (!vertexShader || !fragmentShader) {
        std::cout << "Could not create shaders." << std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    vsg::DescriptorSetLayoutBindings descriptorBindings{ { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                           VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 128 } };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{ 0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },  // vertex data
        VkVertexInputBindingDescription{ 1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },  // colour data
        VkVertexInputBindingDescription{ 2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX }   // tex coord data
    };
    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },  // vertex data
        VkVertexInputAttributeDescription{ 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },  // colour data
        VkVertexInputAttributeDescription{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 },     // tex coord data
    };
    vsg::GraphicsPipelineStates pipelineStates{ vsg::VertexInputState::create(vertexBindingsDescriptions,
                                                                              vertexAttributeDescriptions),
                                                vsg::InputAssemblyState::create(),
                                                vsg::RasterizationState::create(),
                                                vsg::MultisampleState::create(),
                                                vsg::ColorBlendState::create(),
                                                vsg::DepthStencilState::create() };

    auto pipelineLayout =
            vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout }, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(
            pipelineLayout, vsg::ShaderStages{ vertexShader, fragmentShader }, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    auto texture = vsg::DescriptorImage::create(colorImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{ texture });
    auto bindDescriptorSet =
            vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    auto transform = vsg::MatrixTransform::create();
    scenegraph->addChild(transform);

    auto vertices = vsg::vec3Array::create({ { -0.5f, -0.5f, 0.0f },
                                             { 0.5f, -0.5f, 0.0f },
                                             { 0.5f, 0.5f, 0.0f },
                                             { -0.5f, 0.5f, 0.0f },
                                             { -0.5f, -0.5f, -0.5f },
                                             { 0.5f, -0.5f, -0.5f },
                                             { 0.5f, 0.5f, -0.5f },
                                             { -0.5f, 0.5f, -0.5f } });
    auto colors = vsg::vec3Array::create({
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
    });
    auto texcoords = vsg::vec2Array::create({ { 0.0f, 0.0f },
                                              { 1.0f, 0.0f },
                                              { 1.0f, 1.0f },
                                              { 0.0f, 1.0f },
                                              { 0.0f, 0.0f },
                                              { 1.0f, 0.0f },
                                              { 1.0f, 1.0f },
                                              { 0.0f, 1.0f } });
    auto indices = vsg::ushortArray::create({ 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 });

    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices, colors, texcoords }));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));
    transform->addChild(drawCommands);

    return scenegraph;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, const VkExtent2D& extent) {
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective =
            vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                     nearFarRatio * radius, radius * 4.5);
    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

class RenderToTextureExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/teapot.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/teapot.vsgt", "models/lz.vsgt" }, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "No valid model files specified." };

        // 模型默认 Y_UP 朝上，转成 Z_UP 场景习惯（maths 示例里那套约定转换的实战）
        auto zUp = vsg::MatrixTransform::create(
                vsg::dmat4(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0));
        zUp->addChild(vsg_scene);
        auto transform = vsg::MatrixTransform::create();
        transform->addChild(zUp);
        vsg_scene = transform;

        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        auto context = vsg::Context::create(window->getOrCreateDevice());
        VkExtent2D targetExtent{ 512, 512 };

        auto offscreenCamera = createCameraForScene(vsg_scene, targetExtent);
        auto colorImage = vsg::ImageInfo::create();
        auto depthImage = vsg::ImageInfo::create();
        auto rtt_RenderGraph = createOffscreenRendergraph(*context, targetExtent, *colorImage, *depthImage);
        auto rtt_view = vsg::View::create(offscreenCamera, vsg_scene);
        rtt_RenderGraph->addChild(rtt_view);
        auto headlight = vsg::createHeadlight();
        rtt_view->addChild(headlight);

        vsg::ref_ptr<vsg::Node> planes = createPlanes(colorImage, options);
        auto camera = createCameraForScene(planes, window->extent2D());
        auto main_RenderGraph = vsg::createRenderGraphForView(window, camera, planes);

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        std::cout << "Single CommandGraph containing by the RTT and main RenderGraphs" << std::endl;
        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(rtt_RenderGraph);   // 先渲纹理
        commandGraph->addChild(main_RenderGraph);  // 再渲窗口（同帧内顺序保证）
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        transform_ = transform;
        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void frame(WindowContext& ctx) override {
        ctx.viewer->handleEvents();

        // 模型持续自转：两块"屏幕"里的画面随之流动
        float time = std::chrono::duration<float, std::chrono::seconds::period>(ctx.viewer->getFrameStamp()->time -
                                                                                start_time_)
                             .count();
        transform_->matrix = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        ctx.viewer->update();
        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

private:
    FileParam* model_ = nullptr;

    vsg::ref_ptr<vsg::MatrixTransform> transform_;
    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(RenderToTextureExample, "vsgrendertotexture", "app",
                        "Render-to-texture loop: the model renders into a 512×512 offscreen texture (usage = "
                        "COLOR_ATTACHMENT | SAMPLED), two quads sample it in the main window while the model spins "
                        "— the feedback pattern behind mirrors, minimaps and portal rendering.");

}  // namespace vsglab::examples
