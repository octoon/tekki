#pragma once

#include <cstdint>
#include <array>
#include <glm/glm.hpp>
#include "render_overrides.h"
#include "view_constants.h"

namespace tekki::rust_shaders_shared {

constexpr size_t IRCACHE_CASCADE_COUNT = 12;

struct GiCascadeConstants {
    glm::ivec4 scroll_frac;
    glm::ivec4 scroll_int;
    glm::ivec4 voxels_scrolled_this_frame;
    float volume_size;
    float voxel_size;
    glm::uvec2 pad;

    GiCascadeConstants() = default;
    GiCascadeConstants(const GiCascadeConstants&) = default;
    GiCascadeConstants& operator=(const GiCascadeConstants&) = default;
};

struct IrcacheCascadeConstants {
    glm::ivec4 origin;
    glm::ivec4 voxels_scrolled_this_frame;

    IrcacheCascadeConstants() = default;
    IrcacheCascadeConstants(const IrcacheCascadeConstants&) = default;
    IrcacheCascadeConstants& operator=(const IrcacheCascadeConstants&) = default;
};

struct FrameConstants {
    ViewConstants view_constants;

    glm::vec4 sun_direction;

    uint32_t frame_index;
    float delta_time_seconds;
    float sun_angular_radius_cos;
    uint32_t triangle_light_count;

    glm::vec4 sun_color_multiplier;
    glm::vec4 sky_ambient;

    float pre_exposure;
    float pre_exposure_prev;
    float pre_exposure_delta;
    float pad0;

    RenderOverrides render_overrides;

    glm::vec4 ircache_grid_center;
    std::array<IrcacheCascadeConstants, IRCACHE_CASCADE_COUNT> ircache_cascades;

    FrameConstants() = default;
    FrameConstants(const FrameConstants&) = default;
    FrameConstants& operator=(const FrameConstants&) = default;
};

} // namespace tekki::rust_shaders_shared