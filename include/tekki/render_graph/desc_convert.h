#pragma once

#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/buffer.h"
#include <glm/glm.hpp>

namespace tekki::render_graph {

// Helper functions to convert descriptors for temporal operations
inline ImageDesc ConvertToTemporalOutput(const ImageDesc& src) {
    ImageDesc desc = src;
    // Temporal output typically needs storage and transfer access
    desc.Usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return desc;
}

inline ImageDesc ConvertToTemporalInput(const ImageDesc& src) {
    ImageDesc desc = src;
    // Temporal input typically needs sampled access
    desc.Usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return desc;
}

// Convert extent for half resolution
inline glm::u32vec3 HalfExtent(const glm::u32vec3& extent) {
    return glm::u32vec3(
        std::max(1u, extent.x / 2),
        std::max(1u, extent.y / 2),
        std::max(1u, extent.z / 2)
    );
}

// Convert extent with divisor
inline glm::u32vec3 DivideExtent(const glm::u32vec3& extent, const glm::u32vec3& divisor) {
    return glm::u32vec3(
        std::max(1u, extent.x / divisor.x),
        std::max(1u, extent.y / divisor.y),
        std::max(1u, extent.z / divisor.z)
    );
}

// Convert extent with div-up (round up division)
inline glm::u32vec3 DivUpExtent(const glm::u32vec3& extent, const glm::u32vec3& divisor) {
    return glm::u32vec3(
        std::max(1u, (extent.x + divisor.x - 1) / divisor.x),
        std::max(1u, (extent.y + divisor.y - 1) / divisor.y),
        std::max(1u, (extent.z + divisor.z - 1) / divisor.z)
    );
}

} // namespace tekki::render_graph