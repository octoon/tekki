#include "tekki/rust_shaders_shared/gbuffer.h"
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <cmath>
#include <stdexcept>

namespace tekki::rust_shaders_shared {

GbufferDataPacked GbufferData::Pack() const {
    try {
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
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to pack GbufferData: " + std::string(e.what()));
    }
}

GbufferData GbufferDataPacked::Unpack() const {
    try {
        glm::vec2 roughnessMetalness = glm::unpackHalf2x16(v.z);
        
        return GbufferData{
            .albedo = UnpackAlbedo(),
            .emissive = Rgb9e5ToFloat3(v.w),
            .normal = UnpackNormal(),
            .roughness = PerceptualRoughnessToRoughness(roughnessMetalness.x),
            .metalness = roughnessMetalness.y
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to unpack GbufferDataPacked: " + std::string(e.what()));
    }
}

glm::vec3 GbufferDataPacked::UnpackNormal() const {
    try {
        return UnpackNormal111011(v.y);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to unpack normal: " + std::string(e.what()));
    }
}

glm::vec3 GbufferDataPacked::UnpackAlbedo() const {
    try {
        return UnpackColor888(v.x);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to unpack albedo: " + std::string(e.what()));
    }
}

glm::vec4 GbufferDataPacked::ToVec4() const {
    try {
        return glm::vec4(
            glm::uintBitsToFloat(v.x),
            glm::uintBitsToFloat(v.y),
            glm::uintBitsToFloat(v.z),
            glm::uintBitsToFloat(v.w)
        );
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to convert to vec4: " + std::string(e.what()));
    }
}

GbufferDataPacked FromUVec4(const glm::uvec4& data0) {
    try {
        return GbufferDataPacked{
            .v = glm::uvec4(data0.x, data0.y, data0.z, data0.w)
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create GbufferDataPacked from uvec4: " + std::string(e.what()));
    }
}

} // namespace tekki::rust_shaders_shared