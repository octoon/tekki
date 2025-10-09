```cpp
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace tekki::rust_shaders_shared {

inline glm::vec2 GetUvU(const glm::uvec2& pix, const glm::vec4& texSize) {
    return (glm::vec2(pix) + glm::vec2(0.5f)) * glm::vec2(texSize.z, texSize.w);
}

// Replacement for abs due to SPIR-V codegen bug. See https://github.com/EmbarkStudios/rust-gpu/issues/468
inline float AbsF32(float x) {
    if (x >= 0.0f) {
        return x;
    } else {
        return -x;
    }
}

// For element `i` of `self`, return `v[i].abs()`
// Work around for https://github.com/EmbarkStudios/rust-gpu/issues/468.
inline glm::vec2 AbsVec2(const glm::vec2& v) {
    return glm::vec2(AbsF32(v.x), AbsF32(v.y));
}

// For element `i` of `self`, return `v[i].abs()`
// Work around for https://github.com/EmbarkStudios/rust-gpu/issues/468.
inline glm::vec3 AbsVec3(const glm::vec3& v) {
    return glm::vec3(AbsF32(v.x), AbsF32(v.y), AbsF32(v.z));
}

// For element `i` of `self`, return `v[i].abs()`
// Work around for https://github.com/EmbarkStudios/rust-gpu/issues/468.
inline glm::vec4 AbsVec4(const glm::vec4& v) {
    return glm::vec4(AbsF32(v.x), AbsF32(v.y), AbsF32(v.z), AbsF32(v.w));
}

inline float FastSqrt(float x) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    u.i = 0x1fbd1df5 + (u.i >> 1);
    return u.f;
}

inline glm::vec3 FastSqrtVec3(const glm::vec3& v) {
    return glm::vec3(FastSqrt(v.x), FastSqrt(v.y), FastSqrt(v.z));
}

// From Eberly 2014
inline float FastAcos(float x) {
    float abs_x = AbsF32(x);
    float res = -0.156583f * abs_x + glm::pi<float>() / 2.0f;
    res *= FastSqrt(1.0f - abs_x);
    if (x >= 0.0f) {
        return res;
    } else {
        return glm::pi<float>() - res;
    }
}

// Replacement for signum due to SPIR-V codegen bug. See https://github.com/EmbarkStudios/rust-gpu/issues/468
inline float SignumF32(float x) {
    if (x > 0.0f) {
        return 1.0f;
    } else if (x < 0.0f) {
        return -1.0f;
    } else {
        return 0.0f;
    }
}

inline float DepthToViewZ(float depth, const FrameConstants& frameConstants) {
    return 1.0f / (depth * -frameConstants.ViewConstants.ClipToView[2][3]);
}

inline glm::vec4 DepthToViewZVec4(const glm::vec4& depth, const FrameConstants& frameConstants) {
    return 1.0f / (depth * -frameConstants.ViewConstants.ClipToView[2][3]);
}

// Note: `const_mat3` is initialized with columns, while `float3x3` in HLSL is row-order,
// therefore the initializers _appear_ transposed compared to HLSL.
// The difference is only in the `top` and `bottom` ones; the others are symmetric.
constexpr glm::mat3 CUBE_MAP_FACE_ROTATIONS[6] = {
    glm::mat3(0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f), // right
    glm::mat3(0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f),   // left
    glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f),   // top
    glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f),   // bottom
    glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f),  // back
    glm::mat3(-1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),  // front
};

inline float RadicalInverseVdc(uint32_t bits) {
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return static_cast<float>(bits) * 2.3283064e-10f; // / 0x100000000
}

inline glm::vec2 Hammersley(uint32_t i, uint32_t n) {
    return glm::vec2(static_cast<float>(i + 1) / static_cast<float>(n), RadicalInverseVdc(i + 1));
}

// Building an Orthonormal Basis, Revisited
// http://jcgt.org/published/0006/01/01/
inline glm::mat3 BuildOrthonormalBasis(const glm::vec3& n) {
    glm::vec3 b1;
    glm::vec3 b2;

    if (n.z < 0.0f) {
        float a = 1.0f / (1.0f - n.z);
        float b = n.x * n.y * a;
        b1 = glm::vec3(1.0f - n.x * n.x * a, -b, n.x);
        b2 = glm::vec3(b, n.y * n.y * a - 1.0f, -n.y);
    } else {
        float a = 1.0f / (1.0f + n.z);
        float b = -n.x * n.y * a;
        b1 = glm::vec3(1.0f - n.x * n.x * a, b, -n.x);
        b2 = glm::vec3(b, 1.0f - n.y * n.y * a, -n.y);
    }

    return glm::mat3(b1, b2, n);
}

inline glm::vec3 UniformSampleCone(const glm::vec2& urand, float cosThetaMax) {
    float cos_theta = (1.0f - urand.x) + urand.x * cosThetaMax;
    float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    float phi = urand.y * glm::two_pi<float>();
    return glm::vec3(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
}

inline glm::vec2 UvToCs(const glm::vec2& uv) {
    return (uv - glm::vec2(0.5f, 0.5f)) * glm::vec2(2.0f, -2.0f);
}

inline glm::vec2 CsToUv(const glm::vec2& cs) {
    return cs * glm::vec2(0.5f, -0.5f) + glm::vec2(0.5f, 0.5f);
}

inline uint32_t PackUnorm(float val, uint32_t bitCount) {
    uint32_t max_val = (1u << bitCount) - 1;
    return static_cast<uint32_t>(std::clamp(val, 0.0f, 1.0f) * static_cast<float>(max_val));
}

inline float UnpackUnorm(uint32_t pckd, uint32_t bitCount) {
    uint32_t max_val = (1u << bitCount) - 1;
    return static_cast<float>(pckd & max_val) / static_cast<float>(max_val);
}

inline glm::vec3 UnpackNormal111011(uint32_t pckd) {
    uint32_t p = pckd;
    return glm::normalize(glm::vec3(
        UnpackUnorm(p, 11),
        UnpackUnorm(p >> 11, 10),
        UnpackUnorm(p >> 21, 11)
    ) * 2.0f - glm::vec3(1.0f));
}

inline uint32_t PackNormal111011(const glm::vec3& n) {
    uint32_t pckd = 0;
    pckd += PackUnorm(n.x * 0.5f + 0.5f, 11);
    pckd += PackUnorm(n.y * 0.5f + 0.5f, 10) << 11;
    pckd += PackUnorm(n.z * 0.5f + 0.5f, 11) << 21;
    return pckd;
}

inline uint32_t PackColor888(const glm::vec3& color) {
    glm::vec3 sqrt_color = FastSqrtVec3(color);
    uint32_t pckd = 0;
    pckd += PackUnorm(sqrt_color.x, 8);
    pckd += PackUnorm(sqrt_color.y, 8) << 8;
    pckd += PackUnorm(sqrt_color.z, 8) << 16;
    return pckd;
}

inline glm::vec3 UnpackColor888(uint32_t p) {
    glm::vec3 color = glm::vec3(
        UnpackUnorm(p, 8),
        UnpackUnorm(p >> 8, 8),
        UnpackUnorm(p >> 16, 8)
    );
    return color * color;
}

inline glm::vec3 UnpackUnitDirection111011(uint32_t pck) {
    return glm::vec3(
        static_cast<float>(pck & ((1u << 11u) - 1u)) * (2.0f / static_cast<float>((1u << 11u) - 1u)) - 1.0f,
        static_cast<float>((pck >> 11u) & ((1u << 10) - 1u)) * (2.0f / static_cast<float>((1u << 10u) - 1u)) - 1.0f,
        static_cast<float>(pck >> 21) * (2.0f / static_cast<float>((1u << 11u) - 1u)) - 1.0f
    );
}

inline uint32_t PackUnitDirection111011(float x, float y, float z) {
    uint32_t x_packed = static_cast<uint32_t>((std::clamp(x, -1.0f, 1.0f) * 0.5f + 0.5f) * static_cast<float>((1u << 11u) - 1u));
    uint32_t y_packed = static_cast<uint32_t>((std::clamp(y, -1.0f, 1.0f) * 0.5f + 0.5f) * static_cast<float>((1u << 10u) - 1u));
    uint32_t z_packed = static_cast<uint32_t>((std::clamp(z, -1.0f, 1.0f) * 0.5f + 0.5f) * static_cast<float>((1u << 11u) - 1u));

    return (z_packed << 21) | (y_packed << 11) | x_packed;
}

// The below functions provide a simulation of ByteAddressBuffer and VertexPacked.

inline glm::vec2 Load2F(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t offset = byteOffset >> 2;
    float a; std::memcpy(&a, &data[offset], sizeof(float));
    float b; std::memcpy(&b, &data[offset + 1], sizeof(float));
    return glm::vec2(a, b);
}

inline glm::vec3 Load3F(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t offset = byteOffset >> 2;
    float a; std::memcpy(&a, &data[offset], sizeof(float));
    float b; std::memcpy(&b, &data[offset + 1], sizeof(float));
    float c; std::memcpy(&c, &data[offset + 2], sizeof(float));
    return glm::vec3(a, b, c);
}

inline glm::vec4 Load4F(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t offset = byteOffset >> 2;
    float a; std::memcpy(&a, &data[offset], sizeof(float));
    float b; std::memcpy(&b, &data[offset + 1], sizeof(float));
    float c; std::memcpy(&c, &data[offset + 2], sizeof(float));
    float d; std::memcpy(&d, &data[offset + 3], sizeof(float));
    return glm::vec4(a, b, c, d);
}

/// Decode mesh vertex from Kajiya ("core", position + normal packed together)
/// The returned normal is not normalized (but close).
inline std::pair<glm::vec3, glm::vec3> LoadVertex(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t core_offset = byteOffset >> 2;
    glm::vec3 in_pos;
    std::memcpy(&in_pos.x, &data[core_offset], sizeof(float));
    std::memcpy(&in_pos.y, &data[core_offset + 1], sizeof(float));
    std::memcpy(&in_pos.z, &data[core_offset + 2], sizeof(float));
    glm::vec3 in_normal = UnpackUnitDirection111011(data[core_offset + 3]);
    return std::make_pair(in_pos, in_normal);
}

inline void StoreVertex(std::vector<uint32_t>& data, uint32_t byteOffset, const glm::vec3& position, const glm::vec3& normal) {
    size_t offset = byteOffset >> 2;
    uint32_t packed_normal = PackUnitDirection111011(normal.x, normal.y, normal.z);
    data[offset] = *reinterpret_cast<const uint32_t*>(&position.x);
    data[offset + 1] = *reinterpret_cast<const uint32_t*>(&position.y);
    data[offset + 2] = *reinterpret_cast<const uint32_t*>(&position.z);
    data[offset + 3] = packed_normal;
}

inline glm::vec4 UnpackU32ToVec4(uint32_t v) {
    return glm::vec4(
        static_cast<float>(v & 0xFF) / 255.0f,
        static_cast<float>((v >> 8) & 0xFF) / 255.0f,
        static_cast<float>((v >> 16) & 0xFF) / 255.0f,
        static_cast<float>((v >> 24) & 0xFF) / 255.0f
    );
}

inline float RoughnessToPerceptualRoughness(float r) {
    return std::sqrt(r);
}

inline float PerceptualRoughnessToRoughness(float r) {
    return r * r;
}

constexpr uint32_t RGB9E5_EXPONENT_BITS = 5;
constexpr uint32_t RGB9E5_MANTISSA_BITS = 9;
constexpr uint32_t RGB9E5_EXP_BIAS = 15;
constexpr uint32_t RGB9E5_MAX_VALID_BIASED_EXP = 31;
constexpr uint32_t MAX_RGB9E5_EXP = RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS;
constexpr uint32_t RGB9E5_MANTISSA_VALUES = 1 <<