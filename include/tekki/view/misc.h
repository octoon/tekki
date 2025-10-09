#pragma once

#include <algorithm>
#include <cmath>

namespace tekki::view {

/**
 * Scale, bias and saturate x to 0..1 range
 * Evaluate polynomial
 */
inline float Smoothstep(float edge0, float edge1, float x) {
    // Scale, bias and saturate x to 0..1 range
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // Evaluate polynomial
    return x * x * (3.0f - 2.0f * x);
}

} // namespace tekki::view