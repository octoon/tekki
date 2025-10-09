#include "tekki/backend/vulkan/mod.h"
#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <stdexcept>
#include <algorithm>
#include "tekki/core/Result.h"
#include "vulkan/vulkan.h"
#include "device.h"
#include "surface.h"
#include "swapchain.h"
#include "physical_device.h"
#include "instance.h"

namespace tekki::backend::vulkan {

VkSurfaceFormatKHR RenderBackend::SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    VkSurfaceFormatKHR preferred = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    auto it = std::find(formats.begin(), formats.end(), preferred);
    if (it != formats.end()) {
        return preferred;
    } else {
        throw std::runtime_error("No suitable surface format found");
    }
}

std::shared_ptr<RenderBackend> RenderBackend::Create(void* window, const Config& config) {
    try {
        auto instance = Instance::Builder()
            .GraphicsDebugging(config.GraphicsDebugging)
            .Build();

        auto surface = Surface::Create(instance, window);

        auto physical_devices = PhysicalDevice::EnumeratePhysicalDevices(instance)
            .WithPresentationSupport(surface);

        // Log available physical devices
        for (const auto& device : physical_devices) {
            // Log device name - implementation depends on logging system
        }

        std::shared_ptr<PhysicalDevice> physical_device;
        if (config.DeviceIndex.has_value()) {
            physical_device = std::make_shared<PhysicalDevice>(physical_devices.at(config.DeviceIndex.value()));
        } else {
            auto best_device = std::max_element(physical_devices.rbegin(), physical_devices.rend(),
                [](const PhysicalDevice& a, const PhysicalDevice& b) {
                    auto score_a = [&]() {
                        switch (a.GetProperties().deviceType) {
                            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 200;
                            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
                            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
                            default: return 0;
                        }
                    }();
                    auto score_b = [&]() {
                        switch (b.GetProperties().deviceType) {
                            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 200;
                            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
                            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
                            default: return 0;
                        }
                    }();
                    return score_a < score_b;
                });
            physical_device = std::make_shared<PhysicalDevice>(*best_device);
        }

        // Log selected physical device
        // Implementation depends on logging system

        auto device = Device::Create(physical_device);
        auto surface_formats = Swapchain::EnumerateSurfaceFormats(device, surface);

        // Log available surface formats
        // Implementation depends on logging system

        Swapchain::Desc swapchain_desc = {
            .format = SelectSurfaceFormat(surface_formats),
            .dims = VkExtent2D{
                .width = config.SwapchainExtent.x,
                .height = config.SwapchainExtent.y
            },
            .vsync = config.Vsync
        };

        auto swapchain = Swapchain::Create(device, surface, swapchain_desc);

        auto render_backend = std::make_shared<RenderBackend>();
        render_backend->Device = device;
        render_backend->Surface = surface;
        render_backend->Swapchain = swapchain;

        return render_backend;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create RenderBackend: ") + e.what());
    }
}

} // namespace tekki::backend::vulkan