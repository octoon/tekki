#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <array>
#include <cstddef>

namespace tekki::asset {

enum class TexGamma {
    Linear,
    Srgb
};

enum class TexCompressionMode {
    None,
    Rgba,
    Rg
};

struct TexParams {
    // Transform parameters
    glm::vec2 scale = glm::vec2(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float rotation = 0.0f;

    // Texture properties
    TexGamma gamma = TexGamma::Srgb;
    bool use_mips = true;
    bool srgb = false;
    bool mipmaps = true;
    bool anisotropic_filtering = true;

    // Compression and swizzling (from mesh.h)
    TexCompressionMode Compression = TexCompressionMode::None;
    std::optional<std::array<size_t, 4>> ChannelSwizzle;

    // Legacy aliases
    TexGamma Gamma() const { return gamma; }
    bool UseMips() const { return use_mips; }
};

} // namespace tekki::asset