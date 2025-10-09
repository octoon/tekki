#pragma once

#include <cstdint>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace tekki::rust_shaders_shared {

struct MeshDescriptor {
    uint32_t VertexCoreOffset; // position, normal packed in one
    uint32_t VertexUvOffset;
    uint32_t VertexMatOffset;
    uint32_t VertexAuxOffset;
    uint32_t VertexTangentOffset;
    uint32_t MatDataOffset;
    uint32_t IndexOffset;
};

struct InstanceDynamicConstants {
    float EmissiveMultiplier;
};

struct TextureMaps {
    glm::uvec4 Data;

    inline uint32_t Normal() const {
        return Data.x;
    }

    inline uint32_t MetallicRoughness() const {
        return Data.y;
    }

    inline uint32_t Albedo() const {
        return Data.z;
    }

    inline uint32_t Emissive() const {
        return Data.w;
    }
};

class TextureMapsBuilder {
private:
    glm::uvec4 Data;

public:
    TextureMapsBuilder() : Data(0, 0, 0, 0) {}

    TextureMapsBuilder WithNormal(uint32_t normal) {
        Data.x = normal;
        return *this;
    }

    TextureMapsBuilder WithMetallicRoughness(uint32_t metallicRoughness) {
        Data.y = metallicRoughness;
        return *this;
    }

    TextureMapsBuilder WithAlbedo(uint32_t albedo) {
        Data.z = albedo;
        return *this;
    }

    TextureMapsBuilder WithEmissive(uint32_t emissive) {
        Data.w = emissive;
        return *this;
    }

    TextureMaps Build() const {
        return TextureMaps{Data};
    }
};

struct MaterialDescriptor {
    glm::vec4 BaseColorMult;
    TextureMaps Maps;
    float RoughnessMult;
    float MetalnessFactor;
    glm::vec4 Emissive;
    uint32_t Flags;
    std::array<std::array<float, 6>, 4> MapTransforms;

    static MaterialDescriptor Load(const uint32_t* data, uint32_t byteOffset) {
        size_t offset = byteOffset >> 2;
        
        glm::vec4 baseColorMult = LoadVec4(data, offset);
        TextureMaps maps{glm::uvec4(
            data[offset + 4],
            data[offset + 5],
            data[offset + 6],
            data[offset + 7]
        )};
        float roughnessMult = glm::uintBitsToFloat(data[offset + 8]);
        float metalnessFactor = glm::uintBitsToFloat(data[offset + 9]);
        glm::vec4 emissive = LoadVec4(data, offset + 10);
        uint32_t flags = data[offset + 15];
        auto mapTransforms = LoadMapTransforms(data, offset + 16);

        return MaterialDescriptor{
            baseColorMult,
            maps,
            roughnessMult,
            metalnessFactor,
            emissive,
            flags,
            mapTransforms
        };
    }

    glm::vec2 TransformUv(glm::vec2 uv, size_t mapIdx) const {
        const auto& mat = MapTransforms[mapIdx];
        glm::mat2 rotScl = glm::mat2(
            glm::vec2(mat[0], mat[2]),
            glm::vec2(mat[1], mat[3])
        );
        glm::vec2 offset = glm::vec2(mat[4], mat[5]);
        return rotScl * uv + offset;
    }

private:
    static glm::vec4 LoadVec4(const uint32_t* data, size_t offset) {
        return glm::vec4(
            glm::uintBitsToFloat(data[offset]),
            glm::uintBitsToFloat(data[offset + 1]),
            glm::uintBitsToFloat(data[offset + 2]),
            glm::uintBitsToFloat(data[offset + 3])
        );
    }

    static std::array<float, 6> LoadF32_6(const uint32_t* data, size_t offset) {
        return std::array<float, 6>{
            glm::uintBitsToFloat(data[offset]),
            glm::uintBitsToFloat(data[offset + 1]),
            glm::uintBitsToFloat(data[offset + 2]),
            glm::uintBitsToFloat(data[offset + 3]),
            glm::uintBitsToFloat(data[offset + 4]),
            glm::uintBitsToFloat(data[offset + 5])
        };
    }

    static std::array<std::array<float, 6>, 4> LoadMapTransforms(const uint32_t* data, size_t offset) {
        return std::array<std::array<float, 6>, 4>{
            LoadF32_6(data, offset),
            LoadF32_6(data, offset + 6),
            LoadF32_6(data, offset + 12),
            LoadF32_6(data, offset + 18)
        };
    }
};

}