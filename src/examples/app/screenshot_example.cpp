/**
 * @file screenshot_example.cpp
 * @brief 移植自 vsgExamples/examples/app/vsgscreenshot
 *
 * 交互式截图：按键 S 抓颜色、D 抓深度。ScreenshotHandler 是自定义事件处理器，
 * 按键置标志 → 渲染循环在 recordAndSubmit 之后（present 之前）执行回读：
 * 交换链图像 PRESENT_SRC→TRANSFER_SRC 布局转换 → BlitImage 到 HOST_VISIBLE
 * 图像 → 专属 fence 同步提交 → map 内存 → 写 .vsgt 文件。
 * 与 vsgheadless 的差异：headless 是离屏自建 framebuffer 主动出图，
 * 本例是从**正在呈现的窗口**上抓取，源图像布局是 PRESENT_SRC_KHR。
 *
 * 与原版的差异：
 * - 模型/输出文件改为界面参数（原版命令行模型 + --cf/--df）。
 * - 去掉了 --use-vkEvent（VkEvent 同步支线）、--vulkan 版本选择、-f、
 *   depthStencilResolve 属性打印块（属于多采样深度解析支线）、--msaa/--float。
 * - 输出用 .vsgb（二进制）；文件落在 exe 目录，按键后在输出区打印绝对路径。
 *
 * @author hxxcxx
 * @date 2026-11-17
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <filesystem>
#include <iostream>
#include <thread>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::FileParam;
using framework::ParamSet;
using framework::WindowContext;

class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler> {
public:
    bool do_image_capture = false;
    bool do_depth_capture = false;
    vsg::Path colorFilename;
    vsg::Path depthFilename;
    vsg::ref_ptr<vsg::Options> options;

    ScreenshotHandler(const vsg::Path& in_colorFilename, const vsg::Path& in_depthFilename,
                      vsg::ref_ptr<vsg::Options> in_options = {})
        : colorFilename(in_colorFilename), depthFilename(in_depthFilename), options(in_options) {}

    void apply(vsg::KeyPressEvent& keyPress) override {
        if (keyPress.keyBase == 's') {
            do_image_capture = true;
        }
        if (keyPress.keyBase == 'd') {
            do_depth_capture = true;
        }
    }

    void screenshot_image(vsg::ref_ptr<vsg::Window> window) {
        do_image_capture = false;

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;
        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();

        auto sourceImage = window->imageView(window->imageIndex(1))->image;
        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat;

        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);
        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_SRGB, &destFormatProperties);
        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);
        if (supportsBlit) {
            targetImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        }
        vsg::info("supportsBlit = ", supportsBlit);

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
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                                  // oldLayout  ← 窗口图像特有
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
        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_READ_BIT,                                      // srcAccessMask
                VK_ACCESS_MEMORY_READ_BIT,                                        // dstAccessMask
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                             // oldLayout
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                                  // newLayout
                VK_QUEUE_FAMILY_IGNORED,                                          // srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                                          // dstQueueFamilyIndex
                sourceImage,                                                      // image
                VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }  // subresourceRange
        );
        auto cmd_transitionFromTransferBarrier =
                vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,                 // srcStageMask
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,                 // dstStageMask
                                             0,                                              // dependencyFlags
                                             transitionDestinationImageToMemoryReadBarrier,  // barrier
                                             transitionSourceImageBackToPresentBarrier       // barrier
                );
        commands->addChild(cmd_transitionFromTransferBarrier);

        // 专属一次性命令缓冲 + fence 提交（与主渲染流分离）
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue,
                                   [&](vsg::CommandBuffer& commandBuffer) { commands->record(commandBuffer); });

        VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(*device, destinationImage->vk(device->deviceID), &subResource, &subResourceLayout);
        auto destDeviceMemory = deviceMemory;

        size_t destRowWidth = width * sizeof(vsg::ubvec4);
        vsg::ref_ptr<vsg::Data> imageData;
        if (destRowWidth == subResourceLayout.rowPitch) {
            imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(
                    destDeviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ targetImageFormat }, width,
                    height);  // deviceMemory, offset, flags and dimensions
        } else {
            auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(destDeviceMemory, subResourceLayout.offset, 0,
                                                                       vsg::Data::Properties{ targetImageFormat },
                                                                       subResourceLayout.rowPitch * height);
            imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Properties{ targetImageFormat });
            for (uint32_t row = 0; row < height; ++row) {
                std::memcpy(imageData->dataPointer(row * width),
                            mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
            }
        }

        if (vsg::write(imageData, colorFilename, options)) {
            std::cout << "Written color buffer to "
                      << std::filesystem::absolute(std::filesystem::path(colorFilename.string())).string() << std::endl;
        } else {
            std::cout << "Failed to write color buffer to " << colorFilename << std::endl;
        }
    }

    void screenshot_depth(vsg::ref_ptr<vsg::Window> window) {
        do_depth_capture = false;

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;
        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();

        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        sourceImage->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkFormat sourceImageFormat = sourceImage->format;
        targetImageFormat = sourceImageFormat;

        auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);
        VkDeviceSize bufferSize = memoryRequirements.size;

        auto destinationBuffer = vsg::createBufferAndMemory(
                device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

        VkImageAspectFlags imageAspectFlags = vsg::computeAspectFlagsForFormat(sourceImageFormat);

        auto commands = vsg::Commands::create();

        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,    // srcAccessMask
                VK_ACCESS_TRANSFER_READ_BIT,                             // dstAccessMask
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,        // oldLayout
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                    // newLayout
                VK_QUEUE_FAMILY_IGNORED,                                 // srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                                 // dstQueueFamilyIndex
                sourceImage,                                             // image
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
        auto cmd_transitionSourceImageToTransferSourceLayoutBarrier =
                vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,                         // dstStageMask
                                             0,                                                      // dependencyFlags
                                             transitionSourceImageToTransferSourceLayoutBarrier,     // barrier
                                             transitionDestinationBufferToTransferWriteBarrier       // barrier
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

        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_READ_BIT,                             // srcAccessMask
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,    // dstAccessMask
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                    // oldLayout
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,        // newLayout
                VK_QUEUE_FAMILY_IGNORED,                                 // srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                                 // dstQueueFamilyIndex
                sourceImage,                                             // image
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
        auto cmd_transitionSourceImageBackToPresentBarrier =
                vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_TRANSFER_BIT,                         // srcStageMask
                                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
                                             0,                                                      // dependencyFlags
                                             transitionSourceImageBackToPresentBarrier,              // barrier
                                             transitionDestinationBufferToMemoryReadBarrier          // barrier
                );
        commands->addChild(cmd_transitionSourceImageBackToPresentBarrier);

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue,
                                   [&](vsg::CommandBuffer& commandBuffer) { commands->record(commandBuffer); });

        auto destinationMemory = destinationBuffer->getDeviceMemory(device->deviceID);
        if (targetImageFormat == VK_FORMAT_D32_SFLOAT || targetImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            auto imageData = vsg::MappedData<vsg::floatArray2D>::create(
                    destinationMemory, 0, 0, vsg::Data::Properties{ targetImageFormat }, width,
                    height);  // deviceMemory, offset, flags and dimensions

            size_t num_set_depth = 0;
            size_t num_unset_depth = 0;
            for (auto& value : *imageData) {
                if (value == 0.0f)
                    ++num_unset_depth;
                else
                    ++num_set_depth;
            }
            std::cout << "num_unset_depth = " << num_unset_depth << std::endl;
            std::cout << "num_set_depth = " << num_set_depth << std::endl;

            if (vsg::write(imageData, depthFilename, options)) {
                std::cout << "Written depth buffer to "
                          << std::filesystem::absolute(std::filesystem::path(depthFilename.string())).string()
                          << std::endl;
            }
        } else {
            auto imageData = vsg::MappedData<vsg::uintArray2D>::create(
                    destinationMemory, 0, 0, vsg::Data::Properties{ targetImageFormat }, width,
                    height);  // deviceMemory, offset, flags and dimensions
            if (vsg::write(imageData, depthFilename)) {
                std::cout << "Written depth buffer to "
                          << std::filesystem::absolute(std::filesystem::path(depthFilename.string())).string()
                          << std::endl;
            }
        }
    }

    VkFormat targetImageFormat{};
};

class ScreenshotExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        model_ = params.addFile("Model", "models/lz.vsgt", "Models (*.vsgt *.vsgb *.gltf *.glb *.obj)",
                                { "models/lz.vsgt", "models/teapot.vsgt" }, Apply::Restart);
        color_file_ =
                params.addFile("Color output (key S)", "screenshot.vsgb", "VSG image (*.vsgb)", {}, Apply::Restart);
        depth_file_ = params.addFile("Depth output (key D)", "depth.vsgb", "VSG image (*.vsgb)", {}, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();

        // 交换链与深度图像都要预留 TRANSFER_SRC 用途，否则截图布局转换非法
        windowTraits->swapchainPreferences.imageUsage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        auto options = ctx.options();

        auto vsg_scene = vsg::read_cast<vsg::Node>(model_->value(), options);
        if (!vsg_scene)
            throw vsg::Exception{ "Please specify a valid 3d model file." };

        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.01;

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

        // 按键 S/D 截图的交互处理器
        screenshot_handler_ =
                ScreenshotHandler::create(vsg::Path(color_file_->value()), vsg::Path(depth_file_->value()), options);
        viewer->addEventHandler(screenshot_handler_);

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        window_ = window;
        ctx.viewer = viewer;
    }

    void frame(WindowContext& ctx) override {
        ctx.viewer->handleEvents();
        ctx.viewer->update();
        ctx.viewer->recordAndSubmit();

        // 在 present 之前回读：此刻渲染结果已写完、图像尚未交给呈现引擎
        if (screenshot_handler_->do_image_capture)
            screenshot_handler_->screenshot_image(window_);
        if (screenshot_handler_->do_depth_capture)
            screenshot_handler_->screenshot_depth(window_);

        ctx.viewer->present();
    }

private:
    FileParam* model_ = nullptr;
    FileParam* color_file_ = nullptr;
    FileParam* depth_file_ = nullptr;

    vsg::ref_ptr<ScreenshotHandler> screenshot_handler_;
    vsg::ref_ptr<vsg::Window> window_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(ScreenshotExample, "vsgscreenshot", "app",
                        "Interactive screenshots from a live window: press S to capture color, D for depth. A "
                        "custom handler sets flags on key press; the readback (PRESENT_SRC layout transitions + "
                        "blit to host memory + dedicated fence) runs between recordAndSubmit and present.");

}  // namespace vsglab::examples
