// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-asset/src/mesh.rs

#pragma once

#include "tekki/core/common.h"
#include "tekki/asset/image.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>
#include <cstdint>

namespace tekki::asset {

/**
 * @brief Material map - either an image or placeholder
 */
struct MeshMaterialMap {
    struct ImageData {
        ImageSource source;
        TexParams params;
    };

    struct PlaceholderData {
        std::array<uint8_t, 4> values;
    };

    std::variant<ImageData, PlaceholderData> data;

    static MeshMaterialMap FromImage(const ImageSource& source, const TexParams& params);
    static MeshMaterialMap FromPlaceholder(const std::array<uint8_t, 4>& values);

    bool IsImage() const { return std::holds_alternative<ImageData>(data); }
    bool IsPlaceholder() const { return std::holds_alternative<PlaceholderData>(data); }
};

/**
 * @brief Mesh material flags
 */
namespace MeshMaterialFlags {
    constexpr uint32_t EMISSIVE_USED_AS_LIGHT = 1;
}

/**
 * @brief Mesh material (CPU-side)
 */
struct MeshMaterial {
    glm::vec4 baseColorMult = glm::vec4(1.0f);
    std::array<uint32_t, 4> maps = {0, 0, 0, 0};  // indices into global map array
    float roughnessMult = 1.0f;
    float metalnessFactor = 0.0f;
    glm::vec3 emissive = glm::vec3(0.0f);
    uint32_t flags = 0;
    std::array<std::array<float, 6>, 4> mapTransforms;  // 2x3 affine transforms for each map

    MeshMaterial();
};

/**
 * @brief Triangle mesh with full vertex data
 */
struct TriangleMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> tangents;
    std::vector<uint32_t> materialIds;  // per vertex
    std::vector<uint32_t> indices;
    std::vector<MeshMaterial> materials;
    std::vector<MeshMaterialMap> maps;
};

/**
 * @brief Packed vertex for GPU
 */
struct PackedVertex {
    glm::vec3 pos;
    uint32_t normal;  // packed 11-10-11 format
};

/**
 * @brief Pack a unit direction into 11-10-11 format
 */
uint32_t PackUnitDirection_11_10_11(float x, float y, float z);

/**
 * @brief Unpack 11-10-11 format to unit direction
 */
glm::vec3 UnpackUnitDirection_11_10_11(uint32_t packed);

/**
 * @brief Packed triangle mesh (GPU-optimized)
 */
struct PackedTriMesh {
    std::vector<PackedVertex> verts;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec4> colors;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialIds;
    std::vector<MeshMaterial> materials;
    std::vector<MeshMaterialMap> maps;
};

/**
 * @brief Pack a triangle mesh for GPU
 */
PackedTriMesh PackTriangleMesh(const TriangleMesh& mesh);

/**
 * @brief glTF loading parameters
 */
struct GltfLoadParams {
    std::filesystem::path path;
    float scale = 1.0f;
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

/**
 * @brief glTF scene loader
 */
class GltfLoader {
public:
    GltfLoader() = default;

    Result<TriangleMesh> Load(const GltfLoadParams& params);

private:
    struct GltfData;

    Result<std::unique_ptr<GltfData>> LoadGltfFile(const std::filesystem::path& path);
    void ProcessNode(
        const void* node,
        const glm::mat4& transform,
        TriangleMesh& outMesh,
        const GltfData& data
    );

    MeshMaterial LoadMaterial(
        const void* material,
        const GltfData& data,
        std::vector<MeshMaterialMap>& outMaps
    );

    void CalculateTangents(
        const std::vector<uint32_t>& indices,
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec2>& uvs,
        std::vector<glm::vec4>& outTangents
    );
};

} // namespace tekki::asset
