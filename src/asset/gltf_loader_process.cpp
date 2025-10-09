// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/mesh.h"
#include "tekki/asset/gltf_importer.h"
#include "tekki/core/log.h"

#include <tiny_gltf.h>
#include <glm/gtc/type_ptr.hpp>

namespace tekki::asset {

// Helper to iterate glTF node tree
template<typename Func>
void IterateNodeTree(
    const tinygltf::Node& node,
    const tinygltf::Model& model,
    const glm::mat4& transform,
    Func&& func
) {
    glm::mat4 nodeTransform = glm::make_mat4(node.matrix.data());
    if (node.matrix.empty()) {
        // Build from TRS
        glm::vec3 translation(0.0f);
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale(1.0f);

        if (node.translation.size() == 3) {
            translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        }
        if (node.rotation.size() == 4) {
            rotation = glm::quat(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2])
            );
        }
        if (node.scale.size() == 3) {
            scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
        }

        nodeTransform = glm::translate(glm::mat4(1.0f), translation) *
                       glm::mat4_cast(rotation) *
                       glm::scale(glm::mat4(1.0f), scale);
    }

    glm::mat4 combinedTransform = transform * nodeTransform;
    func(node, combinedTransform);

    for (int childIdx : node.children) {
        IterateNodeTree(model.nodes[childIdx], model, combinedTransform, func);
    }
}

void GltfLoader::ProcessNode(
    const void* nodePtr,
    const glm::mat4& transform,
    TriangleMesh& outMesh,
    const GltfData& data
) {
    const auto& node = *static_cast<const tinygltf::Node*>(nodePtr);

    if (node.mesh < 0) return;

    const auto& mesh = data.model.meshes[node.mesh];
    bool flipWinding = glm::determinant(transform) < 0.0f;

    for (const auto& primitive : mesh.primitives) {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
            TEKKI_LOG_WARN("Skipping non-triangle primitive");
            continue;
        }

        size_t materialIdx = outMesh.materials.size();

        // Load material
        std::vector<MeshMaterialMap> materialMaps;
        MeshMaterial material = LoadMaterial(&data.model.materials[primitive.material], data, materialMaps);

        // Offset map indices
        uint32_t mapBase = static_cast<uint32_t>(outMesh.maps.size());
        for (auto& mapIdx : material.maps) {
            mapIdx += mapBase;
        }

        outMesh.materials.push_back(material);
        outMesh.maps.insert(outMesh.maps.end(), materialMaps.begin(), materialMaps.end());

        // Get vertex attributes
        const auto& attributes = primitive.attributes;

        // Positions (required)
        auto posIt = attributes.find("POSITION");
        if (posIt == attributes.end()) {
            TEKKI_LOG_WARN("Primitive missing POSITION attribute");
            continue;
        }

        const auto& posAccessor = data.model.accessors[posIt->second];
        const auto& posBufferView = data.model.bufferViews[posAccessor.bufferView];
        const auto& posBuffer = data.buffers[posBufferView.buffer];
        const float* positions = reinterpret_cast<const float*>(
            posBuffer.data() + posBufferView.byteOffset + posAccessor.byteOffset
        );
        size_t vertexCount = posAccessor.count;

        // Normals (required)
        auto normIt = attributes.find("NORMAL");
        const float* normals = nullptr;
        if (normIt != attributes.end()) {
            const auto& normAccessor = data.model.accessors[normIt->second];
            const auto& normBufferView = data.model.bufferViews[normAccessor.bufferView];
            const auto& normBuffer = data.buffers[normBufferView.buffer];
            normals = reinterpret_cast<const float*>(
                normBuffer.data() + normBufferView.byteOffset + normAccessor.byteOffset
            );
        }

        // UVs (optional)
        auto uvIt = attributes.find("TEXCOORD_0");
        const float* uvs = nullptr;
        if (uvIt != attributes.end()) {
            const auto& uvAccessor = data.model.accessors[uvIt->second];
            const auto& uvBufferView = data.model.bufferViews[uvAccessor.bufferView];
            const auto& uvBuffer = data.buffers[uvBufferView.buffer];
            uvs = reinterpret_cast<const float*>(
                uvBuffer.data() + uvBufferView.byteOffset + uvAccessor.byteOffset
            );
        }

        // Tangents (optional)
        auto tangIt = attributes.find("TANGENT");
        const float* tangents = nullptr;
        bool hasTangents = false;
        if (tangIt != attributes.end()) {
            const auto& tangAccessor = data.model.accessors[tangIt->second];
            const auto& tangBufferView = data.model.bufferViews[tangAccessor.bufferView];
            const auto& tangBuffer = data.buffers[tangBufferView.buffer];
            tangents = reinterpret_cast<const float*>(
                tangBuffer.data() + tangBufferView.byteOffset + tangAccessor.byteOffset
            );
            hasTangents = true;
        }

        // Colors (optional)
        auto colorIt = attributes.find("COLOR_0");
        const float* colors = nullptr;
        if (colorIt != attributes.end()) {
            const auto& colorAccessor = data.model.accessors[colorIt->second];
            const auto& colorBufferView = data.model.bufferViews[colorAccessor.bufferView];
            const auto& colorBuffer = data.buffers[colorBufferView.buffer];
            colors = reinterpret_cast<const float*>(
                colorBuffer.data() + colorBufferView.byteOffset + colorAccessor.byteOffset
            );
        }

        // Indices
        std::vector<uint32_t> indices;
        if (primitive.indices >= 0) {
            const auto& idxAccessor = data.model.accessors[primitive.indices];
            const auto& idxBufferView = data.model.bufferViews[idxAccessor.bufferView];
            const auto& idxBuffer = data.buffers[idxBufferView.buffer];

            indices.resize(idxAccessor.count);
            const uint8_t* idxData = idxBuffer.data() + idxBufferView.byteOffset + idxAccessor.byteOffset;

            if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* idx16 = reinterpret_cast<const uint16_t*>(idxData);
                for (size_t i = 0; i < idxAccessor.count; ++i) {
                    indices[i] = idx16[i];
                }
            } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* idx32 = reinterpret_cast<const uint32_t*>(idxData);
                std::copy(idx32, idx32 + idxAccessor.count, indices.begin());
            } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* idx8 = idxData;
                for (size_t i = 0; i < idxAccessor.count; ++i) {
                    indices[i] = idx8[i];
                }
            }
        } else {
            // No indices - generate sequential
            indices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; ++i) {
                indices[i] = static_cast<uint32_t>(i);
            }
        }

        // Flip winding if needed
        if (flipWinding) {
            for (size_t i = 0; i < indices.size(); i += 3) {
                std::swap(indices[i], indices[i + 2]);
            }
        }

        // Build vertex data and transform
        uint32_t baseVertex = static_cast<uint32_t>(outMesh.positions.size());

        std::vector<glm::vec3> localPositions(vertexCount);
        std::vector<glm::vec3> localNormals(vertexCount);
        std::vector<glm::vec2> localUvs(vertexCount);
        std::vector<glm::vec4> localTangents(vertexCount);
        std::vector<glm::vec4> localColors(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            // Transform position
            glm::vec4 pos(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2], 1.0f);
            localPositions[i] = glm::vec3(transform * pos);

            // Transform normal
            if (normals) {
                glm::vec4 norm(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2], 0.0f);
                localNormals[i] = glm::normalize(glm::vec3(transform * norm));
            } else {
                localNormals[i] = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            // UVs
            if (uvs) {
                localUvs[i] = glm::vec2(uvs[i * 2], uvs[i * 2 + 1]);
            } else {
                localUvs[i] = glm::vec2(0.0f);
            }

            // Tangents
            if (tangents) {
                glm::vec4 tang(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]);
                glm::vec3 transformedTangent = glm::normalize(glm::vec3(transform * glm::vec4(tang.x, tang.y, tang.z, 0.0f)));
                float handedness = tang.w * (flipWinding ? -1.0f : 1.0f);
                localTangents[i] = glm::vec4(transformedTangent, handedness);
            } else {
                localTangents[i] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            }

            // Colors
            if (colors) {
                localColors[i] = glm::vec4(colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]);
            } else {
                localColors[i] = glm::vec4(1.0f);
            }
        }

        // Calculate tangents if not provided and we have UVs
        if (!hasTangents && uvs) {
            CalculateTangents(indices, localPositions, localNormals, localUvs, localTangents);
        }

        // Offset indices
        for (auto& idx : indices) {
            idx += baseVertex;
        }

        // Append to mesh
        outMesh.positions.insert(outMesh.positions.end(), localPositions.begin(), localPositions.end());
        outMesh.normals.insert(outMesh.normals.end(), localNormals.begin(), localNormals.end());
        outMesh.uvs.insert(outMesh.uvs.end(), localUvs.begin(), localUvs.end());
        outMesh.tangents.insert(outMesh.tangents.end(), localTangents.begin(), localTangents.end());
        outMesh.colors.insert(outMesh.colors.end(), localColors.begin(), localColors.end());
        outMesh.indices.insert(outMesh.indices.end(), indices.begin(), indices.end());

        // Material IDs (per vertex)
        outMesh.materialIds.insert(outMesh.materialIds.end(), vertexCount, static_cast<uint32_t>(materialIdx));
    }
}

Result<TriangleMesh> GltfLoader::Load(const GltfLoadParams& params) {
    auto dataResult = LoadGltfFile(params.path);
    if (!dataResult) {
        return Err<TriangleMesh>(dataResult.Error());
    }

    auto& data = *dataResult.Value();
    TriangleMesh mesh;

    // Find default scene
    int sceneIndex = (data.model.defaultScene >= 0) ? data.model.defaultScene : 0;
    if (sceneIndex >= static_cast<int>(data.model.scenes.size())) {
        return Err<TriangleMesh>("No valid scene in glTF");
    }

    const auto& scene = data.model.scenes[sceneIndex];

    // Build transform
    glm::mat4 transform = glm::scale(
        glm::mat4(1.0f),
        glm::vec3(params.scale)
    );
    transform = transform * glm::mat4_cast(params.rotation);

    // Process all nodes
    for (int nodeIdx : scene.nodes) {
        IterateNodeTree(
            data.model.nodes[nodeIdx],
            data.model,
            transform,
            [this, &mesh, &data](const tinygltf::Node& node, const glm::mat4& xform) {
                ProcessNode(&node, xform, mesh, data);
            }
        );
    }

    TEKKI_LOG_INFO("Loaded glTF mesh: {} vertices, {} indices, {} materials",
        mesh.positions.size(), mesh.indices.size(), mesh.materials.size());

    return Ok(std::move(mesh));
}

} // namespace tekki::asset
