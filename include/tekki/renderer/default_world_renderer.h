#pragma once

#include "world/WorldRenderer.h"
#include "../backend/vulkan/device.h"

#include <memory>
#include <array>

namespace tekki::renderer {

// Create a WorldRenderer with default resources
// This includes:
// - BRDF FG LUT (index 0)
// - Blue noise texture (index 1)
// - Bezold-Brücke LUT (index 2)
std::unique_ptr<world::WorldRenderer> create_default_world_renderer(
    const std::array<uint32_t, 2>& render_extent,
    const std::array<uint32_t, 2>& temporal_upscale_extent,
    std::shared_ptr<vulkan::Device> device
);

} // namespace tekki::renderer
