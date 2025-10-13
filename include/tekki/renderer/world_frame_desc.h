#pragma once

#include <memory>
#include <glm/glm.hpp>
#include "tekki/renderer/camera.h"

namespace tekki::renderer {

/**
 * World frame descriptor
 * Ported from kajiya's WorldFrameDesc struct
 */
struct WorldFrameDesc {
    CameraMatrices CameraMatrices_;

    // Internal render resolution, before any upsampling
    glm::uvec2 RenderExtent;

    // Direction towards the sun
    glm::vec3 SunDirection;

    WorldFrameDesc()
        : RenderExtent(1920, 1080),
          SunDirection(0.0f, 1.0f, 0.0f) {}
};

} // namespace tekki::renderer
