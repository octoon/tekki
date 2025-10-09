#include "tekki/rust_shaders/color.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <stdexcept>

namespace tekki::rust_shaders {

glm::vec3 lin_srgb_to_ycbcr(const glm::vec3& col) {
    // NOTE! This matrix needs to be transposed from the HLSL equivalent.
    const glm::mat3 M = glm::mat3(
        0.2126f, -0.1146f, 0.5f,
        0.7152f, -0.3854f, -0.4542f,
        0.0722f, 0.5f, -0.0458f
    );
    return M * col;
}

glm::vec3 ycbcr_to_lin_srgb(const glm::vec3& col) {
    // NOTE! This matrix needs to be transposed from the HLSL equivalent.
    const glm::mat3 M = glm::mat3(
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1873f, 1.8556f,
        1.5748f, -0.4681f, 0.0f
    );
    return M * col;
}

/// Convert linear sRGB color to monochrome luminance value
float lin_srgb_to_luminance(const glm::vec3& color_lin_srgb) {
    const glm::vec3 coefficients(0.2126f, 0.7152f, 0.0722f);
    return glm::dot(coefficients, color_lin_srgb);
}

} // namespace tekki::rust_shaders