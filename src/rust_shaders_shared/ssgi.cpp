#include "tekki/rust_shaders_shared/ssgi.h"

namespace tekki::rust_shaders_shared {

SsgiConstants::SsgiConstants() = default;

SsgiConstants SsgiConstants::DefaultWithSize(const glm::vec4& inputTexSize, const glm::vec4& outputTexSize) {
    return SsgiConstants{
        inputTexSize,
        outputTexSize,
        1,
        6,
        0.4f,
        0,
        0,
        60.0f
    };
}

SsgiConstants SsgiConstants::InsaneQualityWithSize(const glm::vec4& inputTexSize, const glm::vec4& outputTexSize) {
    return SsgiConstants{
        inputTexSize,
        outputTexSize,
        0,
        32,
        100.0f,
        1,
        1,
        5.0f
    };
}

} // namespace tekki::rust_shaders_shared