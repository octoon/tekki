#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <cstdint>
#include <vector>
#include <memory>
#include "tekki/rust_shaders_shared/util.h"

namespace tekki::rust_shaders_shared {

// Forward declaration
struct GbufferData;

struct GbufferDataPacked {
    glm::uvec4 v;

    GbufferData Unpack() const;
    glm::vec3 UnpackNormal() const;
    glm::vec3 UnpackAlbedo() const;
    glm::vec4 ToVec4() const;
};

struct GbufferData {
    glm::vec3 albedo{0.0f, 0.0f, 0.0f};
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    glm::vec3 normal{0.0f, 0.0f, 0.0f};
    float roughness = 0.0f;
    float metalness = 0.0f;

    GbufferData() = default;

    GbufferDataPacked Pack() const;
};

inline GbufferDataPacked GbufferData::Pack() const {
    float packed_normal = PackNormal111011(normal);
    return GbufferDataPacked{
        glm::uvec4(
            PackColor888(albedo),
            *reinterpret_cast<const uint32_t*>(&packed_normal),
            glm::packHalf2x16(glm::vec2(
                RoughnessToPerceptualRoughness(roughness),
                metalness
            )),
            Float3ToRgb9e5(emissive)
        )
    };
}

inline GbufferData GbufferDataPacked::Unpack() const {
    glm::vec2 roughnessMetalness = glm::unpackHalf2x16(v.z);

    GbufferData result;
    result.albedo = UnpackAlbedo();
    result.emissive = Rgb9e5ToFloat3(v.w);
    result.normal = UnpackNormal();
    result.roughness = PerceptualRoughnessToRoughness(roughnessMetalness.x);
    result.metalness = roughnessMetalness.y;
    return result;
}

inline glm::vec3 GbufferDataPacked::UnpackNormal() const {
    float packed_as_float = *reinterpret_cast<const float*>(&v.y);
    return UnpackNormal111011(packed_as_float);
}

inline glm::vec3 GbufferDataPacked::UnpackAlbedo() const {
    return UnpackColor888(v.x);
}

inline glm::vec4 GbufferDataPacked::ToVec4() const {
    return glm::vec4(
        glm::uintBitsToFloat(v.x),
        glm::uintBitsToFloat(v.y),
        glm::uintBitsToFloat(v.z),
        glm::uintBitsToFloat(v.w)
    );
}


inline GbufferDataPacked FromUVec4(const glm::uvec4& data0) {
    return GbufferDataPacked{
        .v = glm::uvec4(data0.x, data0.y, data0.z, data0.w)
    };
}

} // namespace tekki::rust_shaders_shared