// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/mesh.h"
#include <glm/gtx/norm.hpp>

namespace tekki::asset {

// Simplified tangent calculation based on mikktspace algorithm
void GltfLoader::CalculateTangents(
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& uvs,
    std::vector<glm::vec4>& outTangents
) {
    size_t vertexCount = positions.size();
    outTangents.resize(vertexCount, glm::vec4(0.0f));

    std::vector<glm::vec3> tan1(vertexCount, glm::vec3(0.0f));
    std::vector<glm::vec3> tan2(vertexCount, glm::vec3(0.0f));

    // Calculate tangent and bitangent for each triangle
    size_t triangleCount = indices.size() / 3;
    for (size_t i = 0; i < triangleCount; ++i) {
        uint32_t i0 = indices[i * 3 + 0];
        uint32_t i1 = indices[i * 3 + 1];
        uint32_t i2 = indices[i * 3 + 2];

        const glm::vec3& v0 = positions[i0];
        const glm::vec3& v1 = positions[i1];
        const glm::vec3& v2 = positions[i2];

        const glm::vec2& w0 = uvs[i0];
        const glm::vec2& w1 = uvs[i1];
        const glm::vec2& w2 = uvs[i2];

        float x1 = v1.x - v0.x;
        float x2 = v2.x - v0.x;
        float y1 = v1.y - v0.y;
        float y2 = v2.y - v0.y;
        float z1 = v1.z - v0.z;
        float z2 = v2.z - v0.z;

        float s1 = w1.x - w0.x;
        float s2 = w2.x - w0.x;
        float t1 = w1.y - w0.y;
        float t2 = w2.y - w0.y;

        float r = 1.0f / (s1 * t2 - s2 * t1);
        if (!std::isfinite(r)) {
            r = 1.0f;
        }

        glm::vec3 sdir(
            (t2 * x1 - t1 * x2) * r,
            (t2 * y1 - t1 * y2) * r,
            (t2 * z1 - t1 * z2) * r
        );

        glm::vec3 tdir(
            (s1 * x2 - s2 * x1) * r,
            (s1 * y2 - s2 * y1) * r,
            (s1 * z2 - s2 * z1) * r
        );

        tan1[i0] += sdir;
        tan1[i1] += sdir;
        tan1[i2] += sdir;

        tan2[i0] += tdir;
        tan2[i1] += tdir;
        tan2[i2] += tdir;
    }

    // Orthogonalize and calculate handedness
    for (size_t i = 0; i < vertexCount; ++i) {
        const glm::vec3& n = normals[i];
        const glm::vec3& t = tan1[i];

        // Gram-Schmidt orthogonalize
        glm::vec3 tangent = glm::normalize(t - n * glm::dot(n, t));

        // Calculate handedness
        float handedness = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;

        outTangents[i] = glm::vec4(tangent, handedness);
    }
}

} // namespace tekki::asset
