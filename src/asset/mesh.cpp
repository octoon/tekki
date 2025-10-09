// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/mesh.h"
#include "tekki/core/log.h"

namespace tekki::asset {

// MeshMaterialMap implementation
MeshMaterialMap MeshMaterialMap::FromImage(const ImageSource& source, const TexParams& params) {
    MeshMaterialMap map;
    ImageData imgData;
    imgData.source = source;
    imgData.params = params;
    map.data = std::move(imgData);
    return map;
}

MeshMaterialMap MeshMaterialMap::FromPlaceholder(const std::array<uint8_t, 4>& values) {
    MeshMaterialMap map;
    PlaceholderData placeholder;
    placeholder.values = values;
    map.data = std::move(placeholder);
    return map;
}

// MeshMaterial implementation
MeshMaterial::MeshMaterial() {
    // Initialize default map transform (identity)
    for (auto& transform : mapTransforms) {
        transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    }
}

// Pack unit direction into 11-10-11 format
uint32_t PackUnitDirection_11_10_11(float x, float y, float z) {
    auto pack = [](float v, uint32_t bits) -> uint32_t {
        float clamped = std::clamp(v, -1.0f, 1.0f);
        float normalized = clamped * 0.5f + 0.5f;
        uint32_t maxVal = (1u << bits) - 1u;
        return static_cast<uint32_t>(normalized * maxVal);
    };

    uint32_t px = pack(x, 11);
    uint32_t py = pack(y, 10);
    uint32_t pz = pack(z, 11);

    return (pz << 21) | (py << 11) | px;
}

// Unpack 11-10-11 format to unit direction
glm::vec3 UnpackUnitDirection_11_10_11(uint32_t packed) {
    auto unpack = [](uint32_t v, uint32_t bits) -> float {
        uint32_t maxVal = (1u << bits) - 1u;
        float normalized = static_cast<float>(v) / maxVal;
        return normalized * 2.0f - 1.0f;
    };

    uint32_t px = packed & 0x7FF;          // 11 bits
    uint32_t py = (packed >> 11) & 0x3FF;  // 10 bits
    uint32_t pz = (packed >> 21) & 0x7FF;  // 11 bits

    return glm::vec3(
        unpack(px, 11),
        unpack(py, 10),
        unpack(pz, 11)
    );
}

// Pack triangle mesh for GPU
PackedTriMesh PackTriangleMesh(const TriangleMesh& mesh) {
    PackedTriMesh packed;

    // Pack vertices
    packed.verts.reserve(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        PackedVertex v;
        v.pos = mesh.positions[i];

        if (i < mesh.normals.size()) {
            const auto& n = mesh.normals[i];
            v.normal = PackUnitDirection_11_10_11(n.x, n.y, n.z);
        } else {
            v.normal = PackUnitDirection_11_10_11(0.0f, 1.0f, 0.0f);
        }

        packed.verts.push_back(v);
    }

    // Copy other vertex attributes
    packed.uvs = mesh.uvs;
    packed.tangents = mesh.tangents;
    packed.colors = mesh.colors;
    packed.indices = mesh.indices;
    packed.materialIds = mesh.materialIds;
    packed.materials = mesh.materials;
    packed.maps = mesh.maps;

    return packed;
}

} // namespace tekki::asset
