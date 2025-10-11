#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cmath>

namespace tekki::rust_shaders {

constexpr float PI = 3.14159265358979323846f;
constexpr float PLANET_RADIUS = 6371000.0f;
constexpr glm::vec3 PLANET_CENTER = glm::vec3(0.0f, -PLANET_RADIUS, 0.0f);
constexpr float ATMOSPHERE_HEIGHT = 100000.0f;
constexpr float RAYLEIGH_HEIGHT = ATMOSPHERE_HEIGHT * 0.08f;
constexpr float MIE_HEIGHT = ATMOSPHERE_HEIGHT * 0.012f;

constexpr glm::vec3 C_RAYLEIGH = glm::vec3(5.802f * 1e-6f, 13.558f * 1e-6f, 33.100f * 1e-6f);
constexpr glm::vec3 C_MIE = glm::vec3(3.996f * 1e-6f, 3.996f * 1e-6f, 3.996f * 1e-6f);
constexpr glm::vec3 C_OZONE = glm::vec3(0.650f * 1e-6f, 1.881f * 1e-6f, 0.085f * 1e-6f);

constexpr float ATMOSPHERE_DENSITY = 1.0f;
constexpr float EXPOSURE = 20.0f;

inline float AtmosphereHeight(const glm::vec3& positionWs) {
    return glm::length(positionWs - PLANET_CENTER) - PLANET_RADIUS;
}

inline float DensityRayleigh(float h) {
    return std::exp(-std::max(0.0f, h / RAYLEIGH_HEIGHT));
}

inline float DensityMie(float h) {
    return std::exp(-std::max(0.0f, h / MIE_HEIGHT));
}

inline float DensityOzone(float h) {
    // The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
    return std::max(0.0f, 1.0f - std::abs(h - 25000.0f) / 15000.0f);
}

inline glm::vec3 AtmosphereDensity(float h) {
    return glm::vec3(DensityRayleigh(h), DensityMie(h), DensityOzone(h));
}

inline glm::vec2 SphereIntersection(glm::vec3 rayO, const glm::vec3& rayD, const glm::vec3& sphereO, float sphereR) {
    rayO -= sphereO;
    float a = glm::dot(rayD, rayD);
    float b = 2.0f * glm::dot(rayO, rayD);
    float c = glm::dot(rayO, rayO) - sphereR * sphereR;
    float d = b * b - 4.0f * a * c;
    if (d < 0.0f) {
        return glm::vec2(-1.0f);
    } else {
        d = std::sqrt(d);
        return glm::vec2(-b - d, -b + d) / (2.0f * a);
    }
}

inline glm::vec2 AtmosphereIntersection(const glm::vec3& rayO, const glm::vec3& rayD) {
    return SphereIntersection(
        rayO,
        rayD,
        PLANET_CENTER,
        PLANET_RADIUS + ATMOSPHERE_HEIGHT
    );
}

/// Optical depth is a unitless measurement of the amount of absorption of a participating medium (such as the atmosphere).
/// This function calculates just that for our three atmospheric elements:
/// R: Rayleigh
/// G: Mie
/// B: Ozone
/// If you find the term "optical depth" confusing, you can think of it as "how much density was found along the ray in total".
inline glm::vec3 IntegrateOpticalDepth(const glm::vec3& rayO, const glm::vec3& rayD) {
    glm::vec2 intersection = AtmosphereIntersection(rayO, rayD);
    float rayLength = intersection.y;

    int sampleCount = 8;
    float stepSize = rayLength / static_cast<float>(sampleCount);

    glm::vec3 opticalDepth = glm::vec3(0.0f);

    int i = 0;
    // Using a while loop here as a workaround for a spirv-cross bug.
    // See https://github.com/EmbarkStudios/rust-gpu/issues/739
    while (i < sampleCount) {
        glm::vec3 localPos = rayO + rayD * (static_cast<float>(i) + 0.5f) * stepSize;
        float localHeight = AtmosphereHeight(localPos);
        glm::vec3 localDensity = AtmosphereDensity(localHeight);

        opticalDepth += localDensity * stepSize;

        i++;
    }

    return opticalDepth;
}

// -------------------------------------
// Phase functions
inline float PhaseRayleigh(float costh) {
    return 3.0f * (1.0f + costh * costh) / (16.0f * PI);
}

inline float PhaseMie(float costh, float g) {
    // g = 0.85)
    g = std::min(g, 0.9381f);
    float k = 1.55f * g - 0.55f * g * g * g;
    float kcosth = k * costh;
    return (1.0f - k * k) / ((4.0f * PI) * (1.0f - kcosth) * (1.0f - kcosth));
}

/// Calculate a luminance transmittance value from optical depth.
inline glm::vec3 Absorb(const glm::vec3& opticalDepth) {
    // Note that Mie results in slightly more light absorption than scattering, about 10%
    return glm::exp(-(opticalDepth.x * C_RAYLEIGH + opticalDepth.y * C_MIE * 1.1f + opticalDepth.z * C_OZONE)
        * ATMOSPHERE_DENSITY);
}

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
inline glm::vec3 IntegrateScattering(
    glm::vec3 rayStart,
    const glm::vec3& rayDir,
    float rayLength,
    const glm::vec3& lightDir,
    const glm::vec3& lightColor,
    glm::vec3& transmittance
) {
    // We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
    // This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
    // float rayHeight = AtmosphereHeight(rayStart);
    // float sampleDistributionExponent = 1 + std::clamp(1 - rayHeight / ATMOSPHERE_HEIGHT, 0.0f, 1.0f) * 8; // Slightly arbitrary max exponent of 9
    // float sampleDistributionExponent = 1 + 8 * std::abs(rayDir.y);
    float sampleDistributionExponent = 5.0f;

    glm::vec2 intersection = AtmosphereIntersection(rayStart, rayDir);

    rayLength = std::min(rayLength, intersection.y);
    if (intersection.x > 0.0f) {
        // Advance ray to the atmosphere entry point
        rayStart += rayDir * intersection.x;
        rayLength -= intersection.x;
    }

    float costh = glm::dot(rayDir, lightDir);
    float phaseR = PhaseRayleigh(costh);
    float phaseM = PhaseMie(costh, 0.85f);

    int sampleCount = 16;

    glm::vec3 opticalDepth = glm::vec3(0.0f);
    glm::vec3 rayleigh = glm::vec3(0.0f);
    glm::vec3 mie = glm::vec3(0.0f);

    float prevRayTime = 0.0f;

    for (int i = 1; i <= sampleCount; i++) {
        float rayTime = std::pow(static_cast<float>(i) / static_cast<float>(sampleCount), sampleDistributionExponent) * rayLength;
        // Because we are distributing the samples exponentially, we have to calculate the step size per sample.
        float stepSize = rayTime - prevRayTime;

        // glm::vec3 localPosition = rayStart + rayDir * rayTime;
        glm::vec3 localPosition = rayStart + rayDir * glm::mix(prevRayTime, rayTime, 0.5f);
        float localHeight = AtmosphereHeight(localPosition);
        glm::vec3 localDensity = AtmosphereDensity(localHeight);

        opticalDepth += localDensity * stepSize;

        // The atmospheric transmittance from rayStart to localPosition
        glm::vec3 viewTransmittance = Absorb(opticalDepth);

        glm::vec3 opticalDepthLight = IntegrateOpticalDepth(localPosition, lightDir);

        // The atmospheric transmittance of light reaching localPosition
        glm::vec3 lightTransmittance = Absorb(opticalDepthLight);

        rayleigh += viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
        mie += viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

        prevRayTime = rayTime;
    }

    transmittance = Absorb(opticalDepth);

    return (rayleigh * C_RAYLEIGH + mie * C_MIE) * lightColor * EXPOSURE;
}

} // namespace tekki::rust_shaders