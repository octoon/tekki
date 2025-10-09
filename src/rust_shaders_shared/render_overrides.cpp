#include "tekki/rust_shaders_shared/render_overrides.h"

namespace tekki::rust_shaders_shared {

// Render override flags for controlling rendering behavior
namespace RenderOverrideFlags {
    constexpr uint32_t FORCE_FACE_NORMALS = 1 << 0;
    constexpr uint32_t NO_NORMAL_MAPS = 1 << 1;
    constexpr uint32_t FLIP_NORMAL_MAP_YZ = 1 << 2;
    constexpr uint32_t NO_METAL = 1 << 3;
}

/**
 * Default constructor
 */
RenderOverrides::RenderOverrides() : flags(0), material_roughness_scale(1.0f) {}

/**
 * Copy constructor
 */
RenderOverrides::RenderOverrides(const RenderOverrides& other) = default;

/**
 * Assignment operator
 */
RenderOverrides& RenderOverrides::operator=(const RenderOverrides& other) = default;

/**
 * Equality comparison operator
 */
bool RenderOverrides::operator==(const RenderOverrides& other) const {
    return flags == other.flags && 
           material_roughness_scale == other.material_roughness_scale;
}

/**
 * Inequality comparison operator
 */
bool RenderOverrides::operator!=(const RenderOverrides& other) const {
    return !(*this == other);
}

/**
 * Check if a specific flag is set
 * @param flag The flag to check
 * @return True if the flag is set, false otherwise
 */
bool RenderOverrides::HasFlag(uint32_t flag) const {
    return (flags & flag) != 0;
}

/**
 * Set or clear a specific flag
 * @param flag The flag to modify
 * @param value True to set the flag, false to clear it
 */
void RenderOverrides::SetFlag(uint32_t flag, bool value) {
    if (value) {
        flags |= flag;
    } else {
        flags &= ~flag;
    }
}

} // namespace tekki::rust_shaders_shared