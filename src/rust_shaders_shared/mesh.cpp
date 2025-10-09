#include "tekki/rust_shaders_shared/mesh.h"
#include <stdexcept>
#include <array>

namespace tekki::rust_shaders_shared {

// MeshDescriptor is already fully defined in the header

// InstanceDynamicConstants is already fully defined in the header

// TextureMaps is already fully defined in the header

// TextureMapsBuilder is already fully defined in the header

// MaterialDescriptor implementation

glm::vec2 MaterialDescriptor::TransformUv(glm::vec2 uv, size_t mapIdx) const {
    if (mapIdx >= 4) {
        throw std::out_of_range("mapIdx must be less than 4");
    }
    
    const auto& mat = MapTransforms[mapIdx];
    glm::mat2 rotScl = glm::mat2(
        glm::vec2(mat[0], mat[2]),
        glm::vec2(mat[1], mat[3])
    );
    glm::vec2 offset = glm::vec2(mat[4], mat[5]);
    return rotScl * uv + offset;
}

glm::vec4 MaterialDescriptor::LoadVec4(const uint32_t* data, size_t offset) {
    if (!data) {
        throw std::invalid_argument("data pointer cannot be null");
    }
    
    return glm::vec4(
        glm::uintBitsToFloat(data[offset]),
        glm::uintBitsToFloat(data[offset + 1]),
        glm::uintBitsToFloat(data[offset + 2]),
        glm::uintBitsToFloat(data[offset + 3])
    );
}

std::array<float, 6> MaterialDescriptor::LoadF32_6(const uint32_t* data, size_t offset) {
    if (!data) {
        throw std::invalid_argument("data pointer cannot be null");
    }
    
    return std::array<float, 6>{
        glm::uintBitsToFloat(data[offset]),
        glm::uintBitsToFloat(data[offset + 1]),
        glm::uintBitsToFloat(data[offset + 2]),
        glm::uintBitsToFloat(data[offset + 3]),
        glm::uintBitsToFloat(data[offset + 4]),
        glm::uintBitsToFloat(data[offset + 5])
    };
}

std::array<std::array<float, 6>, 4> MaterialDescriptor::LoadMapTransforms(const uint32_t* data, size_t offset) {
    if (!data) {
        throw std::invalid_argument("data pointer cannot be null");
    }
    
    return std::array<std::array<float, 6>, 4>{
        LoadF32_6(data, offset),
        LoadF32_6(data, offset + 6),
        LoadF32_6(data, offset + 12),
        LoadF32_6(data, offset + 18)
    };
}

MaterialDescriptor MaterialDescriptor::Load(const uint32_t* data, uint32_t byteOffset) {
    if (!data) {
        throw std::invalid_argument("data pointer cannot be null");
    }
    
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

}