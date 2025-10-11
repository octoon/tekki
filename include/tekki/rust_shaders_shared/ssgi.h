#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace tekki::rust_shaders_shared {

struct SsgiConstants {
    glm::vec4 InputTexSize;
    glm::vec4 OutputTexSize;
    uint8_t UseAoOnly;
    uint32_t SsgiHalfSampleCount;
    float MaxKernelRadiusCs;
    uint8_t UseKernelDistanceScaling;
    uint8_t UseRandomJitter;
    float KernelRadius;

    SsgiConstants() = default;

    SsgiConstants(const glm::vec4& inputTexSize, const glm::vec4& outputTexSize,
                  uint8_t useAoOnly, uint32_t ssgiHalfSampleCount, float maxKernelRadiusCs,
                  uint8_t useKernelDistanceScaling, uint8_t useRandomJitter, float kernelRadius)
        : InputTexSize(inputTexSize)
        , OutputTexSize(outputTexSize)
        , UseAoOnly(useAoOnly)
        , SsgiHalfSampleCount(ssgiHalfSampleCount)
        , MaxKernelRadiusCs(maxKernelRadiusCs)
        , UseKernelDistanceScaling(useKernelDistanceScaling)
        , UseRandomJitter(useRandomJitter)
        , KernelRadius(kernelRadius) {}

    static SsgiConstants DefaultWithSize(const glm::vec4& inputTexSize, const glm::vec4& outputTexSize) {
        return SsgiConstants(inputTexSize, outputTexSize, 1, 6, 0.4f, 0, 0, 60.0f);
    }

    static SsgiConstants InsaneQualityWithSize(const glm::vec4& inputTexSize, const glm::vec4& outputTexSize) {
        return SsgiConstants(inputTexSize, outputTexSize, 0, 32, 100.0f, 1, 1, 5.0f);
    }
};

} // namespace tekki::rust_shaders_shared