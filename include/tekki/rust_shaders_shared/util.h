#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace tekki::rust_shaders_shared {

// Forward declaration
struct FrameConstants;

inline glm::vec2 GetUvU(const glm::uvec2& pix, const glm::vec4& texSize) {
    return (glm::vec2(pix) + glm::vec2(0.5f)) * glm::vec2(texSize.z, texSize.w);
}

// Replacement for abs due to SPIR-V codegen bug. See https://github.com/EmbarkStudios/rust-gpu/issues/468
inline float AbsF32(float x) {
    return (x >= 0.0f) ? x : -x;
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
    float res = -0.156583f * abs_x + glm::half_pi<float>();
    res *= FastSqrt(1.0f - abs_x);
    return (x >= 0.0f) ? res : (glm::pi<float>() - res);
}

// Replacement for signum due to SPIR-V codegen bug. See https://github.com/EmbarkStudios/rust-gpu/issues/468
inline float SignumF32(float x) {
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

inline float DepthToViewZ(float depth, const FrameConstants& frameConstants);
inline glm::vec4 DepthToViewZVec4(const glm::vec4& depth, const FrameConstants& frameConstants);

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
    glm::vec3 b1, b2;

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

inline glm::vec3 UnpackNormal111011(float pckd) {
    uint32_t p = *reinterpret_cast<const uint32_t*>(&pckd);
    return glm::normalize(glm::vec3(
        UnpackUnorm(p, 11),
        UnpackUnorm(p >> 11, 10),
        UnpackUnorm(p >> 21, 11)
    ) * 2.0f - glm::vec3(1.0f));
}

inline float PackNormal111011(const glm::vec3& n) {
    uint32_t pckd = 0;
    pckd += PackUnorm(n.x * 0.5f + 0.5f, 11);
    pckd += PackUnorm(n.y * 0.5f + 0.5f, 10) << 11;
    pckd += PackUnorm(n.z * 0.5f + 0.5f, 11) << 21;
    return *reinterpret_cast<const float*>(&pckd);
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
    float a = *reinterpret_cast<const float*>(&data[offset]);
    float b = *reinterpret_cast<const float*>(&data[offset + 1]);
    return glm::vec2(a, b);
}

inline glm::vec3 Load3F(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t offset = byteOffset >> 2;
    float a = *reinterpret_cast<const float*>(&data[offset]);
    float b = *reinterpret_cast<const float*>(&data[offset + 1]);
    float c = *reinterpret_cast<const float*>(&data[offset + 2]);
    return glm::vec3(a, b, c);
}

inline glm::vec4 Load4F(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t offset = byteOffset >> 2;
    float a = *reinterpret_cast<const float*>(&data[offset]);
    float b = *reinterpret_cast<const float*>(&data[offset + 1]);
    float c = *reinterpret_cast<const float*>(&data[offset + 2]);
    float d = *reinterpret_cast<const float*>(&data[offset + 3]);
    return glm::vec4(a, b, c, d);
}

/// Decode mesh vertex from Kajiya ("core", position + normal packed together)
/// The returned normal is not normalized (but close).
inline std::pair<glm::vec3, glm::vec3> LoadVertex(const std::vector<uint32_t>& data, uint32_t byteOffset) {
    size_t core_offset = byteOffset >> 2;
    glm::vec3 in_pos(
        *reinterpret_cast<const float*>(&data[core_offset]),
        *reinterpret_cast<const float*>(&data[core_offset + 1]),
        *reinterpret_cast<const float*>(&data[core_offset + 2])
    );
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
constexpr uint32_t RGB9E5_MANTISSA_VALUES = 1 << RGB9E5_MANTISSA_BITS;
constexpr uint32_t MAX_RGB9E5_MANTISSA = RGB9E5_MANTISSA_VALUES - 1;
constexpr float MAX_RGB9E5 = (static_cast<float>(MAX_RGB9E5_MANTISSA) / static_cast<float>(RGB9E5_MANTISSA_VALUES)) * static_cast<float>(1 << MAX_RGB9E5_EXP);

inline float ClampRangeForRgb9e5(float x) {
    return std::clamp(x, 0.0f, MAX_RGB9E5);
}

inline int32_t FloorLog2Positive(float x) {
    // float bit hacking. Wonder if .log2().floor() wouldn't be faster.
    uint32_t f = *reinterpret_cast<const uint32_t*>(&x);
    return static_cast<int32_t>(f >> 23) - 127;
}

// workaround rust-gpu bug, will be fixed by #690
inline int32_t MyMax(int32_t a, int32_t b) {
    return (a >= b) ? a : b;
}

// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
inline uint32_t Float3ToRgb9e5(const glm::vec3& rgb) {
    float rc = ClampRangeForRgb9e5(rgb.x);
    float gc = ClampRangeForRgb9e5(rgb.y);
    float bc = ClampRangeForRgb9e5(rgb.z);

    float maxrgb = std::max(rc, std::max(gc, bc));
    int32_t exp_shared = MyMax(-(static_cast<int32_t>(RGB9E5_EXP_BIAS)) - 1, FloorLog2Positive(maxrgb)) + 1 + static_cast<int32_t>(RGB9E5_EXP_BIAS);
    float denom = std::exp2(static_cast<float>(exp_shared - static_cast<int32_t>(RGB9E5_EXP_BIAS) - static_cast<int32_t>(RGB9E5_MANTISSA_BITS)));

    int32_t maxm = static_cast<int32_t>(std::floor(maxrgb / denom + 0.5f));
    if (maxm == static_cast<int32_t>(MAX_RGB9E5_MANTISSA + 1)) {
        denom *= 2.0f;
        exp_shared += 1;
    }

    uint32_t rm = static_cast<uint32_t>(std::floor(rc / denom + 0.5f));
    uint32_t gm = static_cast<uint32_t>(std::floor(gc / denom + 0.5f));
    uint32_t bm = static_cast<uint32_t>(std::floor(bc / denom + 0.5f));

    return (rm << (32 - 9)) | (gm << (32 - 9 * 2)) | (bm << (32 - 9 * 3)) | static_cast<uint32_t>(exp_shared);
}

inline uint32_t BitfieldExtract(uint32_t value, uint32_t offset, uint32_t bits) {
    uint32_t mask = (1 << bits) - 1;
    return (value >> offset) & mask;
}

inline glm::vec3 Rgb9e5ToFloat3(uint32_t v) {
    int32_t exponent = static_cast<int32_t>(BitfieldExtract(v, 0, RGB9E5_EXPONENT_BITS)) - static_cast<int32_t>(RGB9E5_EXP_BIAS) - static_cast<int32_t>(RGB9E5_MANTISSA_BITS);
    float scale = std::exp2(static_cast<float>(exponent));

    return glm::vec3(
        static_cast<float>(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS, RGB9E5_MANTISSA_BITS)) * scale,
        static_cast<float>(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS * 2, RGB9E5_MANTISSA_BITS)) * scale,
        static_cast<float>(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS * 3, RGB9E5_MANTISSA_BITS)) * scale
    );
}

inline uint32_t Hash1(uint32_t x) {
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

inline uint32_t HashCombine2(uint32_t x, uint32_t y) {
    constexpr uint32_t M = 1664525;
    constexpr uint32_t C = 1013904223;
    uint32_t seed = (x * M + y + C) * M;
    // Tempering (from Matsumoto)
    seed ^= seed >> 11;
    seed ^= (seed << 7) & 0x9d2c5680;
    seed ^= (seed << 15) & 0xefc60000;
    seed ^= seed >> 18;
    return seed;
}

inline uint32_t Hash2(const glm::uvec2& v) {
    return HashCombine2(v.x, Hash1(v.y));
}

inline uint32_t Hash3(const glm::uvec3& v) {
    return HashCombine2(v.x, Hash2(glm::uvec2(v.y, v.z)));
}

inline float UintToU01Float(uint32_t h) {
    constexpr uint32_t MANTISSA_MASK = 0x007FFFFF;
    constexpr uint32_t ONE = 0x3F800000;
    uint32_t bits = (h & MANTISSA_MASK) | ONE;
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result - 1.0f;
}

inline float Sign(float val) {
    return static_cast<float>((0.0f < val) - (val < 0.0f));
}

} // namespace tekki::rust_shaders_shared
