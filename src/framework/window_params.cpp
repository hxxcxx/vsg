#include "framework/window_params.h"

#include <algorithm>

namespace vsglab::framework {

WindowParams::WindowParams() {
    debug_layer_ = params_.addBool("Validation layer (--debug)", false, Apply::Restart);
    api_dump_layer_ = params_.addBool("API dump layer (--api)", false, Apply::Restart);
    synchronization_layer_ = params_.addBool("Synchronization layer (--sync)", false, Apply::Restart);
    image_count_ = params_.addChoice("Swapchain images", { "2", "3" }, "3", Apply::Restart);
    present_mode_ = params_.addChoice("Present mode", { "FIFO", "FIFO_RELAXED", "MAILBOX", "IMMEDIATE" }, "FIFO",
                                      Apply::Restart);
    surface_format_ = params_.addChoice("Surface format", { "sRGB", "UNORM" }, "sRGB", Apply::Restart);
    samples_ = params_.addChoice("MSAA samples", { "1", "2", "4", "8" }, "1", Apply::Restart);
    device_preference_ =
            params_.addChoice("GPU preference", { "Default", "Discrete GPU", "Integrated GPU", "Virtual GPU", "CPU" },
                              "Default", Apply::Restart);
}

void WindowParams::applyTo(vsg::WindowTraits& traits) const {
    traits.debugLayer = debug_layer_->value();
    traits.apiDumpLayer = api_dump_layer_->value();
    traits.synchronizationLayer = synchronization_layer_->value();

    traits.swapchainPreferences.imageCount = static_cast<uint32_t>(std::stoi(image_count_->value()));

    const auto& mode = present_mode_->value();
    if (mode == "IMMEDIATE")
        traits.swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    else if (mode == "MAILBOX")
        traits.swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    else if (mode == "FIFO_RELAXED")
        traits.swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    else
        traits.swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (surface_format_->value() == "UNORM")
        traits.swapchainPreferences.surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    else
        traits.swapchainPreferences.surfaceFormat = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    switch (std::stoi(samples_->value())) {
    case 2: traits.samples = VK_SAMPLE_COUNT_2_BIT; break;
    case 4: traits.samples = VK_SAMPLE_COUNT_4_BIT; break;
    case 8: traits.samples = VK_SAMPLE_COUNT_8_BIT; break;
    default: traits.samples = VK_SAMPLE_COUNT_1_BIT; break;
    }

    // 与 WindowTraits(CommandLine) 中 --prefer-* 的处理一致：把偏好类型挪到列表最前
    const auto& pref = device_preference_->value();
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    if (pref == "Discrete GPU")
        type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    else if (pref == "Integrated GPU")
        type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    else if (pref == "Virtual GPU")
        type = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
    else if (pref == "CPU")
        type = VK_PHYSICAL_DEVICE_TYPE_CPU;

    if (type != VK_PHYSICAL_DEVICE_TYPE_OTHER) {
        auto& prefs = traits.deviceTypePreferences;
        prefs.erase(std::remove(prefs.begin(), prefs.end(), type), prefs.end());
        prefs.insert(prefs.begin(), type);
    }
}

}  // namespace vsglab::framework
