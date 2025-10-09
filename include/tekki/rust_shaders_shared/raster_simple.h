#pragma once

#include <cstdint>

namespace tekki::rust_shaders_shared {

/**
 * RasterConstants
 */
struct RasterConstants {
    uint32_t DrawIndex;
    uint32_t MeshIndex;
};

} // namespace tekki::rust_shaders_shared