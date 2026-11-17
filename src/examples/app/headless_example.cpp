/**
 * @file headless_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgheadless
 *       （含 Robert Osfield / Tim Moore 及 Sascha Willems 衍生代码的 MIT 许可，
 *         版权与许可声明保留于本文件头部 editor-fold 区）
 *
 * 无窗口离屏渲染：自建 Instance/Device/Framebuffer（颜色 + 深度两附件），
 * 每帧渲染后用 ImageMemoryBarrier + BlitImage/CopyImageToBuffer 把结果搬回
 * HOST_VISIBLE 内存，映射后写成 screenshot.vsgb / depth.vsgb。
 * 这就是"服务器出图/缩略图/云渲染"的最小原型。
 *
 * 与原版的差异：
 * - 作为 Console 示例运行（无窗口、跑完即止，输出到实例标签页）。
 * - 尺寸/MSAA/深度开关/俯视/帧数/输出文件改为界面参数；帧数默认 10（原版 100，
 *   每帧都会写一次文件，10 帧足够验证）。输出的 .vsgb 落在 exe 所在目录，
 *   运行结束会在输出区打印绝对路径。
 * - 去掉了 --debug/--api（校验层）、--pager、-p（相机动画）、--resize
 *   （运行中按节奏重建 framebuffer 的支线）、--use-ec（SecondaryCommandGraph，
 *   由 commands/vsgexecutecommands 正式讲）、--gs（几何着色器）、--st。
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

#include "framework/console_example.h"
#include "framework/example_registry.h"

#include <vsg/all.h>

#include <filesystem>
#include <iostream>
#include <thread>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::ConsoleContext;
using framework::FileParam;
using framework::IntParam;
using framework::ParamSet;

vsg::ref_ptr<vsg::RenderPass> createOffscreenRenderPass(vsg::Device* device, VkFormat imageFormat, VkFormat depthFormat,
                                                        bool requiresDepthRead) {
    auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
    auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;  // difference from createRenderPass
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
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
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

vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent,
                                                  VkFormat imageFormat, VkSampleCountFlagBits samples) {
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent = VkExtent3D{ extent.width, extent.height, 1 };
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = samples;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
}

vsg::ref_ptr<vsg::ImageView> createDepthImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent,
                                                  VkFormat depthFormat, VkSampleCountFlagBits samples) {
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = VkExtent3D{ extent.width, extent.height, 1 };
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = samples;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->flags = 0;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return vsg::createImageView(device, depthImage, vsg::computeAspectFlagsForFormat(depthFormat));
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Image>> createColorCapture(
        vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage,
        VkFormat sourceImageFormat) {
    auto width = extent.width;
    auto height = extent.height;

    auto physicalDevice = device->getPhysicalDevice();

    VkFormat targetImageFormat = sourceImageFormat;
    VkFormatProperties srcFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);
    VkFormatProperties destFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

    bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                        ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

    if (supportsBlit) {
        targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }

    auto destinationImage = vsg::Image::create();
    destinationImage->imageType = VK_IMAGE_TYPE_2D;
    destinationImage->format = targetImageFormat;
    destinationImage->extent.width = width;
    destinationImage->extent.height = height;
    destinationImage->extent.depth = 1;
    destinationImage->arrayLayers = 1;
    destinationImage->mipLevels = 1;
    destinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
    destinationImage->tiling = VK_IMAGE_TILING_LINEAR;
    destinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    destinationImage->compile(device);

    auto deviceMemory =
            vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID),
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    destinationImage->bind(deviceMemory, 0);

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
    auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,                                        // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                      // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                             // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                             // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                          // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                          // dstQueueFamilyIndex
            sourceImage,                                                      // image
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }  // subresourceRange
    );
    auto cmd_transitionForTransferBarrier =
            vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,                        // srcStageMask
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,                        // dstStageMask
                                         0,                                                     // dependencyFlags
                                         transitionDestinationImageToDestinationLayoutBarrier,  // barrier
                                         transitionSourceImageToTransferSourceLayoutBarrier     // barrier
            );
    commands->addChild(cmd_transitionForTransferBarrier);

    if (supportsBlit) {
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
        region.srcOffsets[1] = VkOffset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };

        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
        region.dstOffsets[1] = VkOffset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };

        auto blitImage = vsg::BlitImage::create();
        blitImage->srcImage = sourceImage;
        blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitImage->dstImage = destinationImage;
        blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitImage->regions.push_back(region);
        blitImage->filter = VK_FILTER_NEAREST;

        commands->addChild(blitImage);
    } else {
        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent.width = width;
        region.extent.height = height;
        region.extent.depth = 1;

        auto copyImage = vsg::CopyImage::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstImage = destinationImage;
        copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyImage->regions.push_back(region);

        commands->addChild(copyImage);
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

    return { commands, destinationImage };
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Buffer>> createDepthCapture(
        vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage,
        VkFormat sourceImageFormat) {
    auto width = extent.width;
    auto height = extent.height;

    auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);
    VkDeviceSize bufferSize = memoryRequirements.size;

    auto destinationBuffer =
            vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                               VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    VkImageAspectFlags imageAspectFlags = vsg::computeAspectFlagsForFormat(sourceImageFormat);

    auto commands = vsg::Commands::create();

    auto transitionSourceImageToTransferSourceLayoutBarrier =
            vsg::ImageMemoryBarrier::create(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // srcAccessMask
                                            VK_ACCESS_TRANSFER_READ_BIT,                           // dstAccessMask
                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,      // oldLayout
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                  // newLayout
                                            VK_QUEUE_FAMILY_IGNORED,  // srcQueueFamilyIndex
                                            VK_QUEUE_FAMILY_IGNORED,  // dstQueueFamilyIndex
                                            sourceImage,              // image
                                            VkImageSubresourceRange{ imageAspectFlags, 0, 1, 0, 1 }  // subresourceRange
            );
    auto transitionDestinationBufferToTransferWriteBarrier =
            vsg::BufferMemoryBarrier::create(VK_ACCESS_MEMORY_READ_BIT,     // srcAccessMask
                                             VK_ACCESS_TRANSFER_WRITE_BIT,  // dstAccessMask
                                             VK_QUEUE_FAMILY_IGNORED,       // srcQueueFamilyIndex
                                             VK_QUEUE_FAMILY_IGNORED,       // dstQueueFamilyIndex
                                             destinationBuffer,             // buffer
                                             0,                             // offset
                                             bufferSize                     // size
            );
    auto cmd_transitionSourceImageToTransferSourceLayoutBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                                                             // dstStageMask
            0,                                                   // dependencyFlags
            transitionSourceImageToTransferSourceLayoutBarrier,  // barrier
            transitionDestinationBufferToTransferWriteBarrier    // barrier
    );
    commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = width;  // need to figure out actual row length from somewhere...
        region.bufferImageHeight = height;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = VkOffset3D{ 0, 0, 0 };
        region.imageExtent = VkExtent3D{ width, height, 1 };

        auto copyImage = vsg::CopyImageToBuffer::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstBuffer = destinationBuffer;
        copyImage->regions.push_back(region);

        commands->addChild(copyImage);
    }

    auto transitionSourceImageBackToPresentBarrier =
            vsg::ImageMemoryBarrier::create(VK_ACCESS_TRANSFER_READ_BIT,                           // srcAccessMask
                                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  // dstAccessMask
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                  // oldLayout
                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,      // newLayout
                                            VK_QUEUE_FAMILY_IGNORED,  // srcQueueFamilyIndex
                                            VK_QUEUE_FAMILY_IGNORED,  // dstQueueFamilyIndex
                                            sourceImage,              // image
                                            VkImageSubresourceRange{ imageAspectFlags, 0, 1, 0, 1 }  // subresourceRange
            );
    auto transitionDestinationBufferToMemoryReadBarrier =
            vsg::BufferMemoryBarrier::create(VK_ACCESS_TRANSFER_WRITE_BIT,  // srcAccessMask
                                             VK_ACCESS_MEMORY_READ_BIT,     // dstAccessMask
                                             VK_QUEUE_FAMILY_IGNORED,       // srcQueueFamilyIndex
                                             VK_QUEUE_FAMILY_IGNORED,       // dstQueueFamilyIndex
                                             destinationBuffer,             // buffer
                                             0,                             // offset
                                             bufferSize                     // size
            );
    auto cmd_transitionSourceImageBackToPresentBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                                                             // srcStageMask
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
            0,                                              // dependencyFlags
            transitionSourceImageBackToPresentBarrier,      // barrier
            transitionDestinationBufferToMemoryReadBarrier  // barrier
    );
    commands->addChild(cmd_transitionSourceImageBackToPresentBarrier);

    return { commands, destinationBuffer };
}

class HeadlessExample : public framework::ConsoleExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        width_ = params.addInt("Width", 2048, 64, 8192, Apply::Restart);
        height_ = params.addInt("Height", 1024, 64, 8192, Apply::Restart);
        frames_ = params.addInt("Frames (-f)", 10, 1, 100, Apply::Restart);
        msaa_ = params.addBool("MSAA x8 (--msaa)", false, Apply::Restart);
        use_depth_ = params.addBool("Depth buffer", true, Apply::Restart);
        above_ = params.addBool("Top-down view (--above)", false, Apply::Restart);
        color_file_ =
                params.addFile("Color output (--cf)", "screenshot.vsgb", "VSG image (*.vsgb)", {}, Apply::Restart);
        depth_file_ = params.addFile("Depth output (--df)", "depth.vsgb", "VSG image (*.vsgb)", {}, Apply::Restart);
    }

    void run(ConsoleContext& ctx) override {
        VkExtent2D extent{ static_cast<uint32_t>(width_->value()), static_cast<uint32_t>(height_->value()) };
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        auto numFrames = static_cast<int>(frames_->value());
        auto options = ctx.options();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "Failed to load model: " + model_->value() };

        VkSampleCountFlagBits samples = msaa_->value() ? VK_SAMPLE_COUNT_8_BIT : VK_SAMPLE_COUNT_1_BIT;
        bool useDepthBuffer = use_depth_->value();
        uint32_t vulkanVersion = VK_API_VERSION_1_0;
        if (samples != VK_SAMPLE_COUNT_1_BIT)
            vulkanVersion = VK_API_VERSION_1_2;

        // 离屏渲染自建 Instance/Device（窗口示例里这些由 Window 代建）
        vsg::Names instanceExtensions;
        vsg::Names validatedNames;
        auto instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);

        auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        if (!physicalDevice || queueFamily < 0)
            throw vsg::Exception{ "Could not create PhysicalDevice" };

        vsg::Names deviceExtensions;
        vsg::QueueSettings queueSettings{ vsg::QueueSetting{ queueFamily, { 1.0 } } };
        auto deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        auto device =
                vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);

        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        auto lookAt = (above_->value()) ? vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 1.5), centre,
                                                              vsg::dvec3(0.0, 1.0, 0.0))
                                        : vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 1.5, 0.0), centre,
                                                              vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel")) {
            perspective = vsg::EllipsoidPerspective::create(
                    lookAt, ellipsoidModel, 30.0,
                    static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio, 0.0);
        } else {
            perspective = vsg::Perspective::create(
                    30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio * radius,
                    radius * 4.5);
        }
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

        vsg::ref_ptr<vsg::Framebuffer> framebuffer;
        vsg::ref_ptr<vsg::ImageView> colorImageView;
        vsg::ref_ptr<vsg::ImageView> depthImageView;
        vsg::ref_ptr<vsg::Commands> colorBufferCapture;
        vsg::ref_ptr<vsg::Image> copiedColorBuffer;
        vsg::ref_ptr<vsg::Commands> depthBufferCapture;
        vsg::ref_ptr<vsg::Buffer> copiedDepthBuffer;

        if (useDepthBuffer) {
            colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
            depthImageView = createDepthImageView(device, extent, depthFormat, VK_SAMPLE_COUNT_1_BIT);

            if (samples == VK_SAMPLE_COUNT_1_BIT) {
                auto renderPass = createOffscreenRenderPass(device, imageFormat, depthFormat, true);
                framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView, depthImageView },
                                                       extent.width, extent.height, 1);
            } else {
                auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);
                auto msaa_depthImageView = createDepthImageView(device, extent, depthFormat, samples);
                auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, depthFormat, samples, true);
                framebuffer = vsg::Framebuffer::create(
                        renderPass,
                        vsg::ImageViews{ msaa_colorImageView, colorImageView, msaa_depthImageView, depthImageView },
                        extent.width, extent.height, 1);
            }
            std::tie(colorBufferCapture, copiedColorBuffer) =
                    createColorCapture(device, extent, colorImageView->image, imageFormat);
            std::tie(depthBufferCapture, copiedDepthBuffer) =
                    createDepthCapture(device, extent, depthImageView->image, depthFormat);
        } else {
            colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
            if (samples == VK_SAMPLE_COUNT_1_BIT) {
                auto renderPass = vsg::createRenderPass(device, imageFormat);
                framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView }, extent.width,
                                                       extent.height, 1);
            } else {
                auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);
                auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, samples);
                framebuffer =
                        vsg::Framebuffer::create(renderPass, vsg::ImageViews{ msaa_colorImageView, colorImageView },
                                                 extent.width, extent.height, 1);
            }
            std::tie(colorBufferCapture, copiedColorBuffer) =
                    createColorCapture(device, extent, colorImageView->image, imageFormat);
        }

        auto renderGraph = vsg::RenderGraph::create();
        renderGraph->framebuffer = framebuffer;
        renderGraph->renderArea.offset = { 0, 0 };
        renderGraph->renderArea.extent = extent;
        renderGraph->setClearValues({ { 1.0f, 1.0f, 0.0f, 0.0f } }, VkClearDepthStencilValue{ 0.0f, 0 });

        auto view = vsg::View::create(camera, vsg_scene);
        view->addChild(vsg::createHeadlight());
        renderGraph->addChild(view);

        auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
        commandGraph->addChild(renderGraph);
        if (colorBufferCapture)
            commandGraph->addChild(colorBufferCapture);
        if (depthBufferCapture)
            commandGraph->addChild(depthBufferCapture);

        auto viewer = vsg::Viewer::create();
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });
        viewer->compile();

        uint64_t waitTimeout = 1999999999;  // 1second in nanoseconds.

        while (viewer->advanceToNextFrame() && (numFrames--) > 0) {
            std::cout << "Frame " << viewer->getFrameStamp()->frameCount << std::endl;

            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();

            if (copiedColorBuffer || copiedDepthBuffer) {
                viewer->waitForFences(0, waitTimeout);

                if (copiedColorBuffer) {
                    VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
                    VkSubresourceLayout subResourceLayout;
                    vkGetImageSubresourceLayout(*device, copiedColorBuffer->vk(device->deviceID), &subResource,
                                                &subResourceLayout);
                    auto deviceMemory = copiedColorBuffer->getDeviceMemory(device->deviceID);

                    size_t destRowWidth = extent.width * sizeof(vsg::ubvec4);
                    vsg::ref_ptr<vsg::Data> imageData;
                    if (destRowWidth == subResourceLayout.rowPitch) {
                        imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset,
                                                                                0, vsg::Data::Properties{ imageFormat },
                                                                                extent.width, extent.height);
                    } else {
                        auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(
                                deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ imageFormat },
                                subResourceLayout.rowPitch * extent.height);
                        imageData = vsg::ubvec4Array2D::create(extent.width, extent.height,
                                                               vsg::Data::Properties{ imageFormat });
                        for (uint32_t row = 0; row < extent.height; ++row) {
                            std::memcpy(imageData->dataPointer(row * extent.width),
                                        mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
                        }
                    }
                    vsg::write(imageData, vsg::Path(color_file_->value()));
                }

                if (copiedDepthBuffer) {
                    auto deviceMemory = copiedDepthBuffer->getDeviceMemory(device->deviceID);
                    if (depthFormat == VK_FORMAT_D32_SFLOAT || depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
                        auto imageData = vsg::MappedData<vsg::floatArray2D>::create(
                                deviceMemory, 0, 0, vsg::Data::Properties{ depthFormat }, extent.width,
                                extent.height);  // deviceMemory, offset, flags and dimensions
                        vsg::write(imageData, vsg::Path(depth_file_->value()));
                    } else {
                        auto imageData = vsg::MappedData<vsg::uintArray2D>::create(
                                deviceMemory, 0, 0, vsg::Data::Properties{ depthFormat }, extent.width,
                                extent.height);  // deviceMemory, offset, flags and dimensions
                        vsg::write(imageData, vsg::Path(depth_file_->value()));
                    }
                }
            }
        }

        // 打印输出文件的绝对路径（相对路径落在 exe 目录下）；深度文件仅在实际写出时打印
        if (!color_file_->value().empty())
            std::cout << "written: " << std::filesystem::absolute(std::filesystem::path(color_file_->value())).string()
                      << std::endl;
        if (use_depth_->value() && !depth_file_->value().empty())
            std::cout << "written: " << std::filesystem::absolute(std::filesystem::path(depth_file_->value())).string()
                      << std::endl;
    }

private:
    FileParam* model_ = nullptr;
    IntParam* width_ = nullptr;
    IntParam* height_ = nullptr;
    IntParam* frames_ = nullptr;
    BoolParam* msaa_ = nullptr;
    BoolParam* use_depth_ = nullptr;
    BoolParam* above_ = nullptr;
    FileParam* color_file_ = nullptr;
    FileParam* depth_file_ = nullptr;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(HeadlessExample, "vsgheadless", "app",
                        "Offscreen rendering without a window: builds its own Instance/Device/Framebuffer, renders "
                        "N frames, then blits color to a host-visible image and copies depth to a buffer — writing "
                        "screenshot.vsgb / depth.vsgb. The minimal prototype for server-side image generation.");

}  // namespace vsglab::examples
