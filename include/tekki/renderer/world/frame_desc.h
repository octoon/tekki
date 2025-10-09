#pragma once

#include <array>
#include <cstdint>

namespace tekki::renderer::world {

// World frame descriptor containing rendering parameters
struct WorldFrameDesc {
    // Render extent (width, height)
    std::array<uint32_t, 2> render_extent;

    // Constructor
    WorldFrameDesc(const std::array<uint32_t, 2>& extent)
        : render_extent(extent) {}

    WorldFrameDesc(uint32_t width, uint32_t height)
        : render_extent{width, height} {}
};

} // namespace tekki::renderer::world