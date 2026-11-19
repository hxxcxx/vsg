/**
 * @file render_to_texture_array_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgrendertotexturearray
 *       （Robert Osfield 2023，MIT 许可声明保留于本文件头部 editor-fold 区）
 *
 * texture2DArray 版 RTT：N 层渲染目标共用一张 array 图像（每层一个
 * baseArrayLayer 偏移的 ImageView + 各自的 RenderGraph/相机），主窗口照常
 * 浏览模型，同时**弹出第二个结果窗口**把 N 层纹理平铺显示。启动时打印
 * CollectStats（View 数 / GraphicsPipeline 数 / 实际 VkPipeline 数）——
 * shareViewID 开关演示"共享 View → 复用管线"的资源优化。
 *
 * 与原版的差异：
 * - 模型改为文件参数（原版命令行模型）。
 * - 去掉了 -t/--test、-f、-p（相机动画）、--hmh/--nf（保持默认）。
 * - --numLayers / -s(shareViewID) 改为界面参数。
 * - 第二个结果窗口由示例自建（真 OS 窗口，含仅缩放的 Trackball）——
 *   试验场首次出现"示例自己开窗口"，其事件经 VSG 自身的消息泵处理。
 *
 * @author hxxcxx
 * @date 2026-11-19
 */

/* <editor-fold desc="MIT License">
Copyright(c) 2023 Robert Osfield
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
</editor-fold> */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <map>
#include <set>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::FileParam;
using framework::IntParam;
using framework::ParamSet;
using framework::WindowContext;

// 把一个已有节点的孩子"嫁接"进本节点的遍历（无自有 children 的轻量转发节点）
class TraverseChildrenOfNode : public vsg::Inherit<vsg::Node, TraverseChildrenOfNode> {
public:
    explicit TraverseChildrenOfNode(vsg::ref_ptr<vsg::Node> in_node) : node(in_node) {}

    vsg::observer_ptr<vsg::Node> node;

    template <class N, class V>
    static void t_traverse(N& in_node, V& visitor) {
        if (auto ref_node = in_node.node.ref_ptr())
            ref_node->traverse(visitor);
    }

    void traverse(vsg::Visitor& visitor) override { t_traverse(*this, visitor); }
    void traverse(vsg::ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
    void traverse(vsg::RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
};

// 事件处理器挂进场景图的载体（编译后由 AssignEventHandlers 收集给 viewer）
class EventNode : public vsg::Inherit<vsg::Node, EventNode> {
public:
    EventNode() {}
    explicit EventNode(const vsg::EventHandlers& in_eventHandlers) : eventHandlers(in_eventHandlers) {}
    explicit EventNode(vsg::ref_ptr<vsg::Visitor> eventHandler) {
        if (eventHandler)
            eventHandlers.push_back(eventHandler);
    }
    vsg::EventHandlers eventHandlers;
};

struct AssignEventHandlers : public vsg::Visitor {
    vsg::Viewer& viewer;
    AssignEventHandlers(vsg::Viewer& in_viewer) : viewer(in_viewer) {}

    void apply(vsg::Object& object) override { object.traverse(*this); }

    void apply(vsg::Node& node) override {
        if (auto eventNode = node.cast<EventNode>()) {
            viewer.addEventHandlers(eventNode->eventHandlers);
        } else {
            node.traverse(*this);
        }
    }
};

vsg::ref_ptr<vsg::Image> createImage(vsg::Context& context, uint32_t width, uint32_t height, uint32_t levels,
                                     VkFormat format, VkImageUsageFlags usage) {
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{ width, height, 1 };
    image->mipLevels = 1;
    image->arrayLayers = levels;  // ★ 一张图 N 层
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image->compile(context);
    return image;
}

vsg::ref_ptr<vsg::RenderGraph> createOffscreenRendergraph(vsg::Context& context, const VkExtent2D& extent,
                                                          uint32_t layer, vsg::ref_ptr<vsg::Image> colorImage,
                                                          vsg::ImageInfo& colorImageInfo,
                                                          vsg::ref_ptr<vsg::Image> depthImage,
                                                          vsg::ImageInfo& depthImageInfo) {
    // 每层一个 ImageView：只圈住 array 图像的第 layer 层
    auto colorImageView = vsg::ImageView::create(colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
    colorImageView->subresourceRange.baseArrayLayer = layer;
    colorImageView->subresourceRange.layerCount = 1;
    colorImageView->compile(context);

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
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorImageInfo.sampler = colorSampler;

    auto depthImageView = vsg::ImageView::create(depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    depthImageView->subresourceRange.baseArrayLayer = layer;
    depthImageView->subresourceRange.layerCount = 1;
    depthImageView->compile(context);
    depthImageInfo.sampler = nullptr;
    depthImageInfo.imageView = depthImageView;
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    vsg::RenderPass::Attachments attachments(2);
    attachments[0].format = colorImage->format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = depthImage->format;
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

    auto renderPass = vsg::RenderPass::create(context.device, attachments, subpassDescription, dependencies);
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

vsg::ref_ptr<vsg::Camera> createCameraForQuads(vsg::Node* scenegraph, const VkExtent2D& extent) {
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 3.5), centre, vsg::dvec3(0.0, 1.0, 0.0));
    auto perspective =
            vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                     nearFarRatio * radius, radius * 4.5);
    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

vsg::ref_ptr<vsg::Node> createQuad(const vsg::vec3& position, const vsg::vec2& size) {
    auto vertices = vsg::vec3Array::create(
            { position + vsg::vec3(0.0f, 0.0f, 0.0f), position + vsg::vec3(size.x, 0.0f, 0.0f),
              position + vsg::vec3(size.x, size.y, 0.0f), position + vsg::vec3(0.0f, size.y, 0.0f) });
    auto colors = vsg::vec3Array::create({
            { 1.0f, 1.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
    });
    auto texcoords = vsg::vec2Array::create({
            { 0.0f, 1.0f },
            { 1.0f, 1.0f },
            { 1.0f, 0.0f },
            { 0.0f, 0.0f },
    });
    auto indices = vsg::ushortArray::create({ 0, 1, 2, 2, 3, 0 });
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices, colors, texcoords }));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));
    return drawCommands;
}

// 结果窗口：N 层纹理平铺成网格（每个 quad 独立 descriptor 绑定各自层）
vsg::ref_ptr<vsg::CommandGraph> createResultsWindow(vsg::ref_ptr<vsg::Device> device, uint32_t width, uint32_t height,
                                                    vsg::ImageInfoList& imageInfos, VkPresentModeKHR presentMode,
                                                    vsg::ref_ptr<vsg::Options> options) {
    auto vertexShader =
            vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/vert_PushConstants.spv", options);
    auto fragmentShader =
            vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/frag_PushConstants.spv", options);
    if (!vertexShader || !fragmentShader) {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    auto resultsWindowTraits = vsg::WindowTraits::create();
    resultsWindowTraits->width = width;
    resultsWindowTraits->height = height;
    resultsWindowTraits->windowTitle = "render to texture results";
    resultsWindowTraits->device = device;
    resultsWindowTraits->swapchainPreferences.presentMode = presentMode;
    auto resultsWindow = vsg::Window::create(resultsWindowTraits);

    vsg::DescriptorSetLayoutBindings descriptorBindings{ { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                           VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 128 } };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{ 0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
        VkVertexInputBindingDescription{ 1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
        VkVertexInputBindingDescription{ 2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX }
    };
    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        VkVertexInputAttributeDescription{ 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        VkVertexInputAttributeDescription{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 },
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

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);

    vsg::vec3 position(0.0f, 0.0f, 0.0f);
    vsg::vec2 size(1.0f, 1.0f);
    float aspecRatio = static_cast<float>(width) / static_cast<float>(height);
    uint32_t nx = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(imageInfos.size()) * aspecRatio)));
    uint32_t col = 0;
    for (auto& imageInfo : imageInfos) {
        auto texture = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{ texture });
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                                graphicsPipeline->layout, 0, descriptorSet);
        auto quadgroup = vsg::StateGroup::create();
        quadgroup->add(bindDescriptorSet);
        quadgroup->addChild(createQuad(position, size));
        scenegraph->addChild(quadgroup);
        col++;
        if (col < nx) {
            position.x += size.x * 1.1f;
        } else {
            col = 0;
            position.x = 0.0f;
            position.y += size.y * 1.1f;
        }
    }

    auto quads_camera = createCameraForQuads(scenegraph, resultsWindow->extent2D());
    auto quads_view = vsg::View::create(quads_camera, scenegraph);

    auto results_RenderGraph = vsg::RenderGraph::create(resultsWindow);
    results_RenderGraph->addChild(quads_view);

    auto results_commandGraph = vsg::CommandGraph::create(resultsWindow);
    results_commandGraph->submitOrder = 1;
    results_commandGraph->addChild(results_RenderGraph);

    // 结果窗口的 Trackball：只留缩放（滚轮），其余键位全禁用
    auto results_trackball = vsg::Trackball::create(quads_camera);
    results_trackball->addWindow(resultsWindow);
    results_trackball->rotateButtonMask = vsg::BUTTON_MASK_OFF;
    results_trackball->turnLeftKey = vsg::KEY_Undefined;
    results_trackball->turnRightKey = vsg::KEY_Undefined;
    results_trackball->pitchUpKey = vsg::KEY_Undefined;
    results_trackball->pitchDownKey = vsg::KEY_Undefined;
    results_trackball->rollLeftKey = vsg::KEY_Undefined;
    results_trackball->rollRightKey = vsg::KEY_Undefined;
    results_commandGraph->addChild(EventNode::create(results_trackball));

    return results_commandGraph;
}

// 启动时的资源统计：View/GraphicsPipeline/VkPipeline 数量（shareViewID 的对照数据）
struct CollectStats : public vsg::ConstVisitor {
    std::map<const vsg::GraphicsPipeline*, uint32_t> pipelines;
    std::map<const vsg::View*, uint32_t> views;

    void apply(const vsg::Object& object) override { object.traverse(*this); }

    void apply(const vsg::BindGraphicsPipeline& bgp) override { ++pipelines[bgp.pipeline.get()]; }

    void apply(const vsg::View& view) override {
        ++views[&view];
        view.traverse(*this);
    }

    void print(std::ostream& out) {
        std::set<uint32_t> viewIDs;
        out << "number of vsg::View " << views.size() << std::endl;
        for (auto& [view, count] : views) {
            out << "   view = " << view << ", viewID = " << view->viewID << ", count = " << count << std::endl;
            viewIDs.insert(view->viewID);
        }

        uint32_t numPipelines = 0;
        out << "number of vsg::GraphicsPipelines " << pipelines.size() << std::endl;
        for (auto& [pipeline, count] : pipelines) {
            out << "   pipeline = " << pipeline << ", count = " << count;
            for (auto viewID : viewIDs) {
                if (pipeline->validated_vk(viewID) != 0) {
                    ++numPipelines;
                    out << " pipeline->vk(" << viewID << ") = " << pipeline->validated_vk(viewID);
                }
            }
            out << std::endl;
        }
        out << "number of vkPipelines = " << numPipelines << std::endl;
    }
};

class RenderToTextureArrayExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/teapot.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/teapot.vsgt", "models/lz.vsgt" }, Apply::Restart);
        num_layers_ = params.addInt("Layers (--numLayers)", 4, 1, 16, Apply::Restart);
        share_view_ = params.addBool("Share view ID (-s)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto vsg_scene = vsg::Group::create();
        if (auto node = vsg::read_cast<vsg::Node>(model_->value(), options)) {
            vsg_scene->addChild(node);
        } else {
            throw vsg::Exception{ "No valid model files specified." };
        }

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
        auto perspective = vsg::Perspective::create(
                30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                0.001 * radius, radius * 4.5);
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        auto main_trackball = vsg::Trackball::create(camera);
        main_trackball->addWindow(window);
        viewer->addEventHandler(main_trackball);

        auto view3D = vsg::View::create(camera, vsg_scene);
        view3D->addChild(vsg::createHeadlight());

        auto context = vsg::Context::create(window->getOrCreateDevice());

        auto main_RenderGraph = vsg::RenderGraph::create(window);
        main_RenderGraph->addChild(view3D);

        auto main_commandGraph = vsg::CommandGraph::create(window);
        main_commandGraph->submitOrder = 0;
        main_commandGraph->addChild(main_RenderGraph);

        vsg::CommandGraphs commandGraphs;
        commandGraphs.push_back(main_commandGraph);

        const uint32_t numLayers = static_cast<uint32_t>(num_layers_->value());
        const bool shareViewID = share_view_->value();

        vsg::ImageInfoList imageInfos;
        VkExtent2D targetExtent{ 512, 512 };
        auto colorImage =
                createImage(*context, targetExtent.width, targetExtent.height, numLayers, VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        auto depthImage = createImage(*context, targetExtent.width, targetExtent.height, numLayers,
                                      VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        // 同一场景喂给每层的离屏 View（TraverseChildrenOfNode 嫁接 view3D 的孩子）
        auto tcon = TraverseChildrenOfNode::create(view3D);

        auto rtt_commandGraph = vsg::CommandGraph::create(window);
        rtt_commandGraph->submitOrder = -1;             // 在主窗口之前渲
        main_commandGraph->addChild(rtt_commandGraph);  // 嵌套在主 CommandGraph 内

        vsg::ref_ptr<vsg::View> first_rrt_view;
        for (uint32_t layer = 0; layer < numLayers; ++layer) {
            auto offscreenCamera = createCameraForScene(vsg_scene, targetExtent);
            auto colorImageInfo = vsg::ImageInfo::create();
            auto depthImageInfo = vsg::ImageInfo::create();
            auto rtt_RenderGraph = createOffscreenRendergraph(*context, targetExtent, layer, colorImage,
                                                              *colorImageInfo, depthImage, *depthImageInfo);
            imageInfos.push_back(colorImageInfo);

            vsg::ref_ptr<vsg::View> rtt_view;
            if (shareViewID) {
                if (first_rrt_view) {
                    // 克隆首个 View → 同 viewID → 每层的管线在设备上只编译一份
                    rtt_view = vsg::View::create(*first_rrt_view);
                    rtt_view->camera = offscreenCamera;
                } else {
                    first_rrt_view = rtt_view = vsg::View::create(offscreenCamera, tcon);
                }
            } else {
                rtt_view = vsg::View::create(offscreenCamera, tcon);
            }
            rtt_RenderGraph->addChild(rtt_view);
            rtt_commandGraph->addChild(rtt_RenderGraph);
        }

        // 第二个真 OS 窗口平铺显示全部层
        commandGraphs.push_back(createResultsWindow(window->getOrCreateDevice(), windowTraits->width / 2,
                                                    windowTraits->height / 2, imageInfos,
                                                    windowTraits->swapchainPreferences.presentMode, options));

        viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

        AssignEventHandlers aeh(*viewer);
        for (auto cg : commandGraphs)
            cg->accept(aeh);

        viewer->compile();

        vsg::visit<CollectStats>(commandGraphs.begin(), commandGraphs.end()).print(std::cout);

        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void stop(WindowContext& ctx) override {
        if (auto fs = ctx.viewer ? ctx.viewer->getFrameStamp() : nullptr) {
            double fps = static_cast<double>(fs->frameCount) /
                         std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - start_time_)
                                 .count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
        }
    }

private:
    FileParam* model_ = nullptr;
    IntParam* num_layers_ = nullptr;
    BoolParam* share_view_ = nullptr;

    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(RenderToTextureArrayExample, "vsgrendertotexturearray", "app",
                        "texture2DArray RTT: N layers share one array image (per-layer ImageViews), each rendered "
                        "by its own offscreen View; a second real OS window tiles all layers. The Share-view "
                        "switch clones the first View so pipelines compile once per viewID — watch the startup "
                        "stats printout.");

}  // namespace vsglab::examples
