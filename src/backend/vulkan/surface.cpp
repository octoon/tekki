#include "tekki/backend/vulkan/surface.h"
#include "tekki/backend/vulkan/instance.h"
#include <memory>
#include <stdexcept>
#include <GLFW/glfw3.h>

namespace tekki::backend::vulkan {

std::shared_ptr<Surface> Surface::Create(
    const std::shared_ptr<Instance>& instance,
    void* window
) {
    try {
        if (!instance) {
            throw std::invalid_argument("Instance cannot be null");
        }
        if (!window) {
            throw std::invalid_argument("Window cannot be null");
        }

        VkSurfaceKHR surface;
        VkResult result = glfwCreateWindowSurface(
            instance->GetRaw(),
            static_cast<GLFWwindow*>(window),
            nullptr,
            &surface
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan surface");
        }

        return std::make_shared<Surface>(surface, instance->GetRaw());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create surface: ") + e.what());
    }
}

Surface::Surface(VkSurfaceKHR surface, VkInstance instance)
    : raw(surface)
    , instanceHandle(instance)
{
}

} // namespace tekki::backend::vulkan