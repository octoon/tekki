#pragma once

#include <memory>
#include <array>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/surface.h"
#include "tekki/backend/vulkan/swapchain.h"

namespace tekki::backend {

struct RenderBackendConfig {
    std::array<uint32_t, 2> SwapchainExtent;
    bool Vsync;
    bool GraphicsDebugging;
    std::optional<size_t> DeviceIndex;

    RenderBackendConfig()
        : SwapchainExtent{1920, 1080}
        , Vsync(true)
        , GraphicsDebugging(false)
        , DeviceIndex(std::nullopt) {}
};

class RenderBackend {
public:
    std::shared_ptr<vulkan::Device> Device;
    std::shared_ptr<vulkan::Surface> Surface;
    vulkan::Swapchain Swapchain;

    RenderBackend() = default;
    ~RenderBackend() = default;

    // Factory method to create a new RenderBackend
    // static std::shared_ptr<RenderBackend> Create(void* window, const RenderBackendConfig& config);

    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;
    RenderBackend(RenderBackend&&) = default;
    RenderBackend& operator=(RenderBackend&&) = default;
};

// Type aliases for convenience
using Device = vulkan::Device;
using Image = vulkan::Image;

} // namespace tekki::backend
