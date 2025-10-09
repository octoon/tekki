#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <cstdint>
#include <vector>
#include <memory>

namespace tekki::rust_shaders_shared {

struct GbufferDataPacked {
    glm::uvec4 v;
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

inline float RoughnessToPerceptualRoughness(float r) {
    return std::sqrt(r);
}

inline float PerceptualRoughnessToRoughness(float r) {
    return r * r;
}

inline GbufferDataPacked GbufferData::Pack() const {
    return GbufferDataPacked{
        glm::uvec4(
            PackColor888(albedo),
            PackNormal111011(normal),
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
    
    return GbufferData{
        .albedo = UnpackAlbedo(),
        .emissive = Rgb9e5ToFloat3(v.w),
        .normal = UnpackNormal(),
        .roughness = PerceptualRoughnessToRoughness(roughnessMetalness.x),
        .metalness = roughnessMetalness.y
    };
}

inline glm::vec3 GbufferDataPacked::UnpackNormal() const {
    return UnpackNormal111011(v.y);
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