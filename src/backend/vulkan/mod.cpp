#include "tekki/backend/vulkan/mod.h"
#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <stdexcept>
#include <algorithm>
#include "tekki/core/result.h"
#include "vulkan/vulkan.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/surface.h"
#include "tekki/backend/vulkan/swapchain.h"
#include "tekki/backend/vulkan/physical_device.h"
#include "tekki/backend/vulkan/instance.h"

namespace tekki::backend::vulkan {

VkSurfaceFormatKHR RenderBackend::SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    VkSurfaceFormatKHR preferred = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    auto it = std::find_if(formats.begin(), formats.end(),
        [&preferred](const VkSurfaceFormatKHR& fmt) {
            return fmt.format == preferred.format && fmt.colorSpace == preferred.colorSpace;
        });
    if (it != formats.end()) {
        return *it;
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

        auto physical_devices = EnumeratePhysicalDevices(instance);
        physical_devices = PhysicalDeviceList::WithPresentationSupport(physical_devices, surface);

        // Log available physical devices
        for ([[maybe_unused]] const auto& device : physical_devices) {
            // Log device name - implementation depends on logging system
        }

        std::shared_ptr<PhysicalDevice> physical_device;
        if (config.DeviceIndex.has_value()) {
            physical_device = std::make_shared<PhysicalDevice>(physical_devices.at(config.DeviceIndex.value()));
        } else {
            auto best_device = std::max_element(physical_devices.rbegin(), physical_devices.rend(),
                [](const PhysicalDevice& a, const PhysicalDevice& b) {
                    auto score_a = [&]() {
                        switch (a.properties.deviceType) {
                            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 200;
                            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
                            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
                            default: return 0;
                        }
                    }();
                    auto score_b = [&]() {
                        switch (b.properties.deviceType) {
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
        auto surface_formats = Swapchain::EnumerateSurfaceFormats(device, *surface);

        // Log available surface formats
        // Implementation depends on logging system

        SwapchainDesc swapchain_desc;
        swapchain_desc.Format = SelectSurfaceFormat(surface_formats);
        swapchain_desc.Dims.width = config.SwapchainExtent.x;
        swapchain_desc.Dims.height = config.SwapchainExtent.y;
        swapchain_desc.Vsync = config.Vsync;

        auto swapchain = std::make_shared<tekki::backend::vulkan::Swapchain>(device, surface, swapchain_desc);

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