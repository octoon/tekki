#include "tekki/rust_shaders_shared/raster_simple.h"

namespace tekki::rust_shaders_shared {

/**
 * RasterConstants
 */
RasterConstants::RasterConstants() : DrawIndex(0), MeshIndex(0) {}

RasterConstants::RasterConstants(uint32_t draw_index, uint32_t mesh_index) 
    : DrawIndex(draw_index), MeshIndex(mesh_index) {}

RasterConstants::RasterConstants(const RasterConstants& other)
    : DrawIndex(other.DrawIndex), MeshIndex(other.MeshIndex) {}

RasterConstants& RasterConstants::operator=(const RasterConstants& other) {
    if (this != &other) {
        DrawIndex = other.DrawIndex;
        MeshIndex = other.MeshIndex;
    }
    return *this;
}

bool RasterConstants::operator==(const RasterConstants& other) const {
    return DrawIndex == other.DrawIndex && MeshIndex == other.MeshIndex;
}

bool RasterConstants::operator!=(const RasterConstants& other) const {
    return !(*this == other);
}

} // namespace tekki::rust_shaders_shared