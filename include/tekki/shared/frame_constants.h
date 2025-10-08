#pragma once

#include <glm/glm.hpp>

namespace tekki::shared {

constexpr size_t IRCACHE_CASCADE_COUNT = 12;

struct IrcacheCascadeConstants {
    glm::ivec4 origin;
    glm::ivec4 voxels_scrolled_this_frame;
};

struct FrameConstants {
    // View constants would go here
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
    glm::mat4 view_projection_matrix;
    glm::mat4 prev_view_projection_matrix;
    glm::vec4 eye_position;
    glm::vec4 viewport_size;

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

    // Render overrides would go here
    uint32_t render_overrides_flags;

    glm::vec4 ircache_grid_center;
    IrcacheCascadeConstants ircache_cascades[IRCACHE_CASCADE_COUNT];
};

} // namespace tekki::shared