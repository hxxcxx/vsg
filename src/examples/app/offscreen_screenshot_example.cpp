/**
 * @file offscreen_screenshot_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgoffscreenshot
 *       （含 Robert Osfield / Tim Moore 及 Sascha Willems 衍生代码的 MIT 许可，
 *         版权与许可声明保留于本文件头部 editor-fold 区）
 *
 * 窗口 + 离屏双渲染：同一个 viewer 里两条 RenderGraph——屏幕照常交互浏览，
 * 按键 S 时临时启用离屏 RenderGraph（独立 framebuffer、独立相机，跟主相机
 * 同视角），渲一帧 → 传输命令拷到 HOST_VISIBLE 图像 → 关闭离屏 → 存盘。
 * 按键 E 把离屏分辨率同步为当前窗口尺寸（运行中重建 framebuffer/视口/相机
 * 纵横比，并用 replaceChild 换掉传输命令——场景图"热更换"的活例子）。
 *
 * 与原版的差异：
 * - 模型/输出文件改为界面参数（原版命令行模型 + --cf）。
 * - 去掉了 -n/--nested 与 -s（离屏与主渲染的三种 CommandGraph 拓扑切换，
 *   固定用默认的"单 CommandGraph + Switch"路径）、-f。
 * - 输出用 .vsgb；文件落在 exe 目录，按键后在输出区打印绝对路径。
 *
 * @author hxxcxx
 * @date 2026-11-17
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

#include <filesystem>
#include <iostream>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

bool supportsBlit(vsg::ref_ptr<vsg::Device> device, VkFormat format) {
    auto physicalDevice = device->getPhysicalDevice();
    VkFormatProperties srcFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), format, &srcFormatProperties);
    VkFormatProperties destFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_SRGB, &destFormatProperties);
    return ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
           ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);
}

vsg::ref_ptr<vsg::Image> createCaptureImage(vsg::ref_ptr<vsg::Device> device, VkFormat sourceFormat,
                                            const VkExtent2D& extent) {
    auto targetFormat = supportsBlit(device, sourceFormat) ? VK_FORMAT_R8G8B8A8_SRGB : sourceFormat;
    auto image = vsg::Image::create();
    image->format = targetFormat;
    image->extent = { extent.width, extent.height, 1 };
    image->arrayLayers = 1;
    image->mipLevels = 1;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_LINEAR;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image->compile(device);
    auto memReqs = image->getMemoryRequirements(device->deviceID);
    auto memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto deviceMemory = vsg::DeviceMemory::create(device, memReqs, memFlags);
    image->bind(deviceMemory, 0);
    return image;
}

VkImageUsageFlags computeUsageFlagsForFormat(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32: return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    default: return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
}

vsg::ref_ptr<vsg::ImageView> createTransferImageView(vsg::ref_ptr<vsg::Device> device, VkFormat format,
                                                     const VkExtent2D& extent, VkSampleCountFlagBits samples) {
    auto image = vsg::Image::create();
    image->format = format;
    image->extent = VkExtent3D{ extent.width, extent.height, 1 };
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = samples;
    image->usage = computeUsageFlagsForFormat(format) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return vsg::createImageView(device, image, vsg::computeAspectFlagsForFormat(format));
}

vsg::ref_ptr<vsg::Commands> createTransferCommands(vsg::ref_ptr<vsg::Device> device,
                                                   vsg::ref_ptr<vsg::Image> sourceImage,
                                                   vsg::ref_ptr<vsg::Image> destinationImage) {
    auto commands = vsg::Commands::create();

    auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
            0,                                                                // srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                        // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                             // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                          // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                          // dstQueueFamilyIndex
            destinationImage,                                                 // image
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }  // subresourceRange
    );
    auto cmd_transitionForTransferBarrier =
            vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
                                         0,                                                    // dependencyFlags
                                         transitionDestinationImageToDestinationLayoutBarrier  // barrier
            );
    commands->addChild(cmd_transitionForTransferBarrier);

    if (sourceImage->format == destinationImage->format &&
        sourceImage->extent.width == destinationImage->extent.width &&
        sourceImage->extent.height == destinationImage->extent.height) {
        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent = destinationImage->extent;
        auto copyImage = vsg::CopyImage::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstImage = destinationImage;
        copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyImage->regions.push_back(region);
        commands->addChild(copyImage);
    } else if (supportsBlit(device, destinationImage->format)) {
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
        region.srcOffsets[1] = VkOffset3D{ static_cast<int32_t>(sourceImage->extent.width),
                                           static_cast<int32_t>(sourceImage->extent.height), 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
        region.dstOffsets[1] = VkOffset3D{ static_cast<int32_t>(destinationImage->extent.width),
                                           static_cast<int32_t>(destinationImage->extent.height), 1 };
        auto blitImage = vsg::BlitImage::create();
        blitImage->srcImage = sourceImage;
        blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitImage->dstImage = destinationImage;
        blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitImage->regions.push_back(region);
        blitImage->filter = VK_FILTER_NEAREST;
        commands->addChild(blitImage);
    } else {
        throw std::runtime_error{ "GPU does not support blit." };
    }

    auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT,                                     // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                        // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                             // oldLayout
            VK_IMAGE_LAYOUT_GENERAL,                                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                          // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                          // dstQueueFamilyIndex
            destinationImage,                                                 // image
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }  // subresourceRange
    );
    auto cmd_transitionFromTransferBarrier =
            vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
                                         0,                                             // dependencyFlags
                                         transitionDestinationImageToMemoryReadBarrier  // barrier
            );
    commands->addChild(cmd_transitionFromTransferBarrier);
    return commands;
}

vsg::ref_ptr<vsg::RenderPass> createTransferRenderPass(vsg::ref_ptr<vsg::Device> device, VkFormat imageFormat,
                                                       VkFormat depthFormat, bool requiresDepthRead) {
    auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;  // <-- difference from vsg::createRenderPass()
    auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);
    if (requiresDepthRead) {
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    }

    vsg::RenderPass::Attachments attachments{ colorAttachment, depthAttachment };

    vsg::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vsg::AttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.colorAttachments.emplace_back(colorAttachmentRef);
    subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

    vsg::RenderPass::Subpasses subpasses{ subpass };

    vsg::SubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorDependency.dependencyFlags = 0;

    vsg::SubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDependency.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{ colorDependency, depthDependency };
    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

vsg::ref_ptr<vsg::Framebuffer> createOffscreenFramebuffer(vsg::ref_ptr<vsg::Device> device,
                                                          vsg::ref_ptr<vsg::ImageView> transferImageView,
                                                          VkSampleCountFlagBits const samples) {
    VkFormat imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    bool requiresDepthRead = false;

    VkExtent2D const extent{ transferImageView->image->extent.width, transferImageView->image->extent.height };

    vsg::ImageViews imageViews;
    if (samples == VK_SAMPLE_COUNT_1_BIT) {
        imageViews.emplace_back(transferImageView);
        imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, VK_SAMPLE_COUNT_1_BIT));
    } else {
        imageViews.emplace_back(createTransferImageView(device, imageFormat, extent, samples));
        imageViews.emplace_back(transferImageView);
        imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, samples));
        if (requiresDepthRead) {
            imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, VK_SAMPLE_COUNT_1_BIT));
        }
    }

    auto renderPass = createTransferRenderPass(device, imageFormat, depthFormat, requiresDepthRead);
    auto framebuffer = vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);
    return framebuffer;
}

std::tuple<vsg::ref_ptr<vsg::Camera>, vsg::ref_ptr<vsg::Perspective>> createCameraForScene(vsg::Node* scenegraph,
                                                                                           const VkExtent2D& extent) {
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;
    auto eye = centre + vsg::dvec3(0.0, -radius * 3.5, 0.0);
    auto target = centre;
    auto up = vsg::dvec3(0.0, 0.0, 1.0);
    auto lookAt = vsg::LookAt::create(eye, target, up);
    double fieldOfViewY = 30.0;
    double aspectRatio = static_cast<double>(extent.width) / static_cast<double>(extent.height);
    double nearDistance = nearFarRatio * radius;
    double farDistance = radius * 4.5;
    auto perspective = vsg::Perspective::create(fieldOfViewY, aspectRatio, nearDistance, farDistance);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
    return std::tie(camera, perspective);
}

void replaceChild(vsg::Switch* toggle, vsg::ref_ptr<vsg::Node> previous, vsg::ref_ptr<vsg::Node> replacement) {
    bool found = false;
    for (auto& child : toggle->children) {
        if (child.node == previous) {
            child.node = replacement;
            found = true;
        }
    }
    if (!found)
        vsg::info("Warning: replaceChild failed to find previous child\n");
    else
        vsg::info("replaceChild succeeded\n");
}

vsg::ref_ptr<vsg::Data> getImageData(vsg::ref_ptr<vsg::Viewer> viewer, vsg::ref_ptr<vsg::Device> device,
                                     vsg::ref_ptr<vsg::Image> captureImage) {
    constexpr uint64_t waitTimeout = 1000000000;  // 1 second
    viewer->waitForFences(0, waitTimeout);
    VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(*device, captureImage->vk(device->deviceID), &subResource, &subResourceLayout);
    auto deviceMemory = captureImage->getDeviceMemory(device->deviceID);
    size_t destRowWidth = captureImage->extent.width * sizeof(vsg::ubvec4);
    vsg::ref_ptr<vsg::Data> imageData;
    if (destRowWidth == subResourceLayout.rowPitch) {
        imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(
                deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ captureImage->format },
                captureImage->extent.width, captureImage->extent.height);
    } else {
        auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(
                deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ captureImage->format },
                subResourceLayout.rowPitch * captureImage->extent.height);
        imageData = vsg::ubvec4Array2D::create(captureImage->extent.width, captureImage->extent.height,
                                               vsg::Data::Properties{ captureImage->format });
        for (uint32_t row = 0; row < captureImage->extent.height; ++row) {
            std::memcpy(imageData->dataPointer(row * captureImage->extent.width),
                        mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
        }
    }
    return imageData;
}

void saveImage(vsg::Path const& filename, vsg::ref_ptr<vsg::Viewer> viewer, vsg::ref_ptr<vsg::Device> device,
               vsg::ref_ptr<vsg::Image> captureImage, vsg::ref_ptr<vsg::Options> options) {
    vsg::info("writing image to file: ", filename);
    auto imageData = getImageData(viewer, device, captureImage);
    vsg::write(imageData, filename, options);
    vsg::info("image saved.");
}

class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler> {
public:
    bool do_sync_extent = false;
    bool do_image_capture = false;

    ScreenshotHandler() {
        vsg::info("press 's' to save offscreen render to file");
        vsg::info("press 'e' to set offscreen render extents to same as display");
    }

    void apply(vsg::KeyPressEvent& keyPress) override {
        if (keyPress.keyBase == 'e') {
            do_sync_extent = true;
        }
        if (keyPress.keyBase == 's') {
            do_image_capture = true;
        }
    }
};

class OffscreenScreenshotExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        capture_file_ =
                params.addFile("Capture output (key S)", "screenshot.vsgb", "VSG image (*.vsgb)", {}, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = ctx.options();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "No valid model files specified." };

        auto transform = vsg::MatrixTransform::create();
        transform->addChild(vsg_scene);
        vsg_scene = transform;

        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        auto [displayCamera, displayPerspective] = createCameraForScene(vsg_scene, window->extent2D());
        auto displayRenderGraph = vsg::createRenderGraphForView(window, displayCamera, vsg_scene);

        auto device = window->getOrCreateDevice();

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(displayCamera));

        // 离屏渲染的目标（默认与窗口同尺寸）
        VkFormat offscreenImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        offscreen_extent_ = VkExtent2D{ windowTraits->width, windowTraits->height };
        samples_ = VK_SAMPLE_COUNT_1_BIT;
        transferImageView_ =
                createTransferImageView(device, offscreenImageFormat, offscreen_extent_, VK_SAMPLE_COUNT_1_BIT);
        captureImage_ = createCaptureImage(device, offscreenImageFormat, offscreen_extent_);
        captureCommands_ = createTransferCommands(device, transferImageView_->image, captureImage_);

        offscreenRenderGraph_ = vsg::RenderGraph::create();
        offscreenRenderGraph_->framebuffer = createOffscreenFramebuffer(device, transferImageView_, samples_);
        offscreenRenderGraph_->renderArea.extent = offscreenRenderGraph_->framebuffer->extent2D();
        offscreenRenderGraph_->setClearValues(VkClearColorValue{ { 0.0f, 0.0f, 0.0f, 0.0f } },
                                              VkClearDepthStencilValue{ 0.0f, 0 });

        // 离屏相机：独立 Perspective，视角与主相机同步
        offscreenCamera_ = vsg::Camera::create();
        offscreenCamera_->viewMatrix = displayCamera->viewMatrix;
        offscreenPerspective_ =
                vsg::Perspective::create(displayPerspective->fieldOfViewY, displayPerspective->aspectRatio,
                                         displayPerspective->nearDistance, displayPerspective->farDistance);
        offscreenCamera_->projectionMatrix = offscreenPerspective_;
        offscreenCamera_->viewportState = vsg::ViewportState::create(window->extent2D());

        // Switch 包住离屏两件套：平时关闭，按 S 时启用一帧
        offscreenSwitch_ = vsg::Switch::create();
        offscreenEnabled_ = false;
        offscreenSwitch_->addChild(offscreenEnabled_, offscreenRenderGraph_);
        offscreenSwitch_->addChild(offscreenEnabled_, captureCommands_);

        auto offscreenView = vsg::View::create(offscreenCamera_, vsg_scene);
        offscreenView->addChild(vsg::createHeadlight());
        offscreenRenderGraph_->addChild(offscreenView);

        screenshot_handler_ = ScreenshotHandler::create();
        viewer->addEventHandler(screenshot_handler_);

        vsg::info("Single CommandGraph containing by the offscreen and main RenderGraphs");
        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(offscreenSwitch_);
        commandGraph->addChild(displayRenderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();
        offscreenSwitch_->setAllChildren(offscreenEnabled_);

        window_ = window;
        device_ = device;
        display_camera_ = displayCamera;
        display_perspective_ = displayPerspective;
        options_ = options;
        ctx.viewer = viewer;
    }

    void frame(WindowContext& ctx) override {
        auto viewer = ctx.viewer;
        viewer->handleEvents();

        if (screenshot_handler_->do_sync_extent) {
            screenshot_handler_->do_sync_extent = false;
            VkExtent2D displayExtent = display_camera_->getRenderArea().extent;
            if (offscreen_extent_.width != displayExtent.width || offscreen_extent_.height != displayExtent.height) {
                auto prevCaptureCommands = captureCommands_;
                offscreen_extent_ = displayExtent;
                offscreenRenderGraph_->renderArea.extent = displayExtent;
                offscreenPerspective_->aspectRatio =
                        static_cast<double>(offscreen_extent_.width) / static_cast<double>(offscreen_extent_.height);
                offscreenCamera_->viewportState->set(0, 0, offscreen_extent_.width, offscreen_extent_.height);
                transferImageView_ = createTransferImageView(device_, VK_FORMAT_B8G8R8A8_SRGB, offscreen_extent_,
                                                             VK_SAMPLE_COUNT_1_BIT);
                captureImage_ = createCaptureImage(device_, VK_FORMAT_B8G8R8A8_SRGB, offscreen_extent_);
                captureCommands_ = createTransferCommands(device_, transferImageView_->image, captureImage_);
                replaceChild(offscreenSwitch_, prevCaptureCommands, captureCommands_);
                offscreenRenderGraph_->framebuffer = createOffscreenFramebuffer(device_, transferImageView_, samples_);
                vsg::info("offscreen render resized to: ", offscreen_extent_.width, "x", offscreen_extent_.height);
            }
        } else if (screenshot_handler_->do_image_capture && !offscreenEnabled_) {
            offscreenPerspective_->fieldOfViewY = display_perspective_->fieldOfViewY;
            offscreenPerspective_->nearDistance = display_perspective_->nearDistance;
            offscreenPerspective_->farDistance = display_perspective_->farDistance;
            offscreenEnabled_ = true;
            offscreenSwitch_->setAllChildren(offscreenEnabled_);
        }

        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        if (screenshot_handler_->do_image_capture && offscreenEnabled_) {
            screenshot_handler_->do_image_capture = false;
            offscreenEnabled_ = false;
            offscreenSwitch_->setAllChildren(offscreenEnabled_);
            saveImage(vsg::Path(capture_file_->value()), viewer, device_, captureImage_, options_);
            std::cout << "written: "
                      << std::filesystem::absolute(std::filesystem::path(capture_file_->value())).string() << std::endl;
        }
    }

private:
    FileParam* model_ = nullptr;
    FileParam* capture_file_ = nullptr;

    vsg::ref_ptr<ScreenshotHandler> screenshot_handler_;
    vsg::ref_ptr<vsg::Window> window_;
    vsg::ref_ptr<vsg::Device> device_;
    vsg::ref_ptr<vsg::Options> options_;
    vsg::ref_ptr<vsg::Camera> display_camera_;
    vsg::ref_ptr<vsg::Perspective> display_perspective_;

    VkExtent2D offscreen_extent_{};
    VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    vsg::ref_ptr<vsg::ImageView> transferImageView_;
    vsg::ref_ptr<vsg::Image> captureImage_;
    vsg::ref_ptr<vsg::Commands> captureCommands_;
    vsg::ref_ptr<vsg::RenderGraph> offscreenRenderGraph_;
    vsg::ref_ptr<vsg::Camera> offscreenCamera_;
    vsg::ref_ptr<vsg::Perspective> offscreenPerspective_;
    vsg::ref_ptr<vsg::Switch> offscreenSwitch_;
    bool offscreenEnabled_ = false;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(OffscreenScreenshotExample, "vsgoffscreenshot", "app",
                        "Display + offscreen dual rendering: browse interactively, press S to render one frame to "
                        "an offscreen framebuffer (own camera, synced view) and save it; press E to rebuild the "
                        "offscreen target at the current window size — hot-swapping scene graph nodes at runtime.");

}  // namespace vsglab::examples
