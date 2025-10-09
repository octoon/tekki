// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/asset.hpp"
#include "tekki/core/log.h"

#include <iostream>

using namespace tekki;
using namespace tekki::asset;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <gltf-file>\n";
        return 1;
    }

    TEKKI_LOG_INFO("Testing asset loading with: {}", argv[1]);

    // Test glTF loading
    GltfLoadParams params;
    params.path = argv[1];
    params.scale = 1.0f;

    GltfLoader loader;
    auto meshResult = loader.Load(params);

    if (!meshResult) {
        TEKKI_LOG_ERROR("Failed to load mesh: {}", meshResult.Error());
        return 1;
    }

    auto& mesh = *meshResult;
    TEKKI_LOG_INFO("Loaded mesh:");
    TEKKI_LOG_INFO("  Vertices: {}", mesh.positions.size());
    TEKKI_LOG_INFO("  Indices: {}", mesh.indices.size());
    TEKKI_LOG_INFO("  Materials: {}", mesh.materials.size());
    TEKKI_LOG_INFO("  Maps: {}", mesh.maps.size());

    // Test packing
    TEKKI_LOG_INFO("Packing mesh...");
    auto packed = PackTriangleMesh(mesh);

    TEKKI_LOG_INFO("Packed mesh:");
    TEKKI_LOG_INFO("  Packed vertices: {}", packed.verts.size());
    TEKKI_LOG_INFO("  Indices: {}", packed.indices.size());

    // Test serialization
    TEKKI_LOG_INFO("Serializing mesh...");
    auto serialized = SerializePackedMesh(packed);
    TEKKI_LOG_INFO("Serialized size: {} bytes", serialized.size());

    TEKKI_LOG_INFO("Asset loading test completed successfully!");

    return 0;
}
