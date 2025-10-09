#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace tekki::rust_shaders {

/**
 * Convert linear sRGB color to YCbCr color space
 * NOTE! This matrix needs to be transposed from the HLSL equivalent.
 */
inline glm::vec3 LinSrgbToYCbCr(const glm::vec3& col) {
    const glm::mat3 M = glm::mat3(
        0.2126f, -0.1146f, 0.5f,
        0.7152f, -0.3854f, -0.4542f,
        0.0722f, 0.5f, -0.0458f
    );
    return M * col;
}

/**
 * Convert YCbCr color space to linear sRGB color
 * NOTE! This matrix needs to be transposed from the HLSL equivalent.
 */
inline glm::vec3 YCbCrToLinSrgb(const glm::vec3& col) {
    const glm::mat3 M = glm::mat3(
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1873f, 1.8556f,
        1.5748f, -0.4681f, 0.0f
    );
    return M * col;
}

/**
 * Convert linear sRGB color to monochrome luminance value
 */
inline float LinSrgbToLuminance(const glm::vec3& colorLinSrgb) {
    const glm::vec3 coefficients(0.2126f, 0.7152f, 0.0722f);
    return glm::dot(coefficients, colorLinSrgb);
}

} // namespace tekki::rust_shaders