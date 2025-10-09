#include "tekki/rust_shaders_shared/frame_constants.h"

namespace tekki::rust_shaders_shared {

IrcacheCascadeConstants::IrcacheCascadeConstants() 
    : origin(0, 0, 0, 0)
    , voxels_scrolled_this_frame(0, 0, 0, 0) {
}

IrcacheCascadeConstants::IrcacheCascadeConstants(const IrcacheCascadeConstants& other)
    : origin(other.origin)
    , voxels_scrolled_this_frame(other.voxels_scrolled_this_frame) {
}

IrcacheCascadeConstants& IrcacheCascadeConstants::operator=(const IrcacheCascadeConstants& other) {
    if (this != &other) {
        origin = other.origin;
        voxels_scrolled_this_frame = other.voxels_scrolled_this_frame;
    }
    return *this;
}

FrameConstants::FrameConstants()
    : view_constants()
    , sun_direction(0.0f, 0.0f, 0.0f, 0.0f)
    , frame_index(0)
    , delta_time_seconds(0.0f)
    , sun_angular_radius_cos(0.0f)
    , triangle_light_count(0)
    , sun_color_multiplier(0.0f, 0.0f, 0.0f, 0.0f)
    , sky_ambient(0.0f, 0.0f, 0.0f, 0.0f)
    , pre_exposure(0.0f)
    , pre_exposure_prev(0.0f)
    , pre_exposure_delta(0.0f)
    , pad0(0.0f)
    , render_overrides()
    , ircache_grid_center(0.0f, 0.0f, 0.0f, 0.0f)
    , ircache_cascades() {
}

FrameConstants::FrameConstants(const FrameConstants& other)
    : view_constants(other.view_constants)
    , sun_direction(other.sun_direction)
    , frame_index(other.frame_index)
    , delta_time_seconds(other.delta_time_seconds)
    , sun_angular_radius_cos(other.sun_angular_radius_cos)
    , triangle_light_count(other.triangle_light_count)
    , sun_color_multiplier(other.sun_color_multiplier)
    , sky_ambient(other.sky_ambient)
    , pre_exposure(other.pre_exposure)
    , pre_exposure_prev(other.pre_exposure_prev)
    , pre_exposure_delta(other.pre_exposure_delta)
    , pad0(other.pad0)
    , render_overrides(other.render_overrides)
    , ircache_grid_center(other.ircache_grid_center)
    , ircache_cascades(other.ircache_cascades) {
}

FrameConstants& FrameConstants::operator=(const FrameConstants& other) {
    if (this != &other) {
        view_constants = other.view_constants;
        sun_direction = other.sun_direction;
        frame_index = other.frame_index;
        delta_time_seconds = other.delta_time_seconds;
        sun_angular_radius_cos = other.sun_angular_radius_cos;
        triangle_light_count = other.triangle_light_count;
        sun_color_multiplier = other.sun_color_multiplier;
        sky_ambient = other.sky_ambient;
        pre_exposure = other.pre_exposure;
        pre_exposure_prev = other.pre_exposure_prev;
        pre_exposure_delta = other.pre_exposure_delta;
        pad0 = other.pad0;
        render_overrides = other.render_overrides;
        ircache_grid_center = other.ircache_grid_center;
        ircache_cascades = other.ircache_cascades;
    }
    return *this;
}

} // namespace tekki::rust_shaders_shared