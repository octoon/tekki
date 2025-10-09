#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

#include "tekki/rust_shaders_shared/camera.h"

namespace tekki::renderer {

struct WorldFrameDesc {
    tekki::rust_shaders_shared::CameraMatrices CameraMatrices;

    /// Internal render resolution, before any upsampling
    glm::u32vec2 RenderExtent;

    /// Direction _towards_ the sun.
    glm::vec3 SunDirection;

    WorldFrameDesc() = default;
    
    WorldFrameDesc(const WorldFrameDesc&) = default;
    WorldFrameDesc& operator=(const WorldFrameDesc&) = default;
};

}