#pragma once

#include <glm/glm.hpp>

namespace tekki::asset {

struct TexParams {
    // Stub implementation for texture parameters
    glm::vec2 scale = glm::vec2(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float rotation = 0.0f;
    bool srgb = false;
    bool mipmaps = true;
    bool anisotropic_filtering = true;
};

} // namespace tekki::asset