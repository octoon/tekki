// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-asset-pipe/src/lib.rs

#pragma once

#include "tekki/core/common.h"
#include "tekki/asset/mesh.h"
#include "tekki/asset/image.h"
#include <filesystem>
#include <memory>
#include <functional>
#include <future>
#include <unordered_map>

namespace tekki::asset {

/**
 * @brief Mesh asset processing parameters
 */
struct MeshAssetProcessParams {
    std::filesystem::path path;
    std::string outputName;
    float scale = 1.0f;
};

/**
 * @brief Asset cache for lazy loading and caching
 */
class AssetCache {
public:
    AssetCache();
    ~AssetCache();

    // Singleton access
    static AssetCache& Instance();

    // Load and cache a mesh
    std::future<Result<std::shared_ptr<PackedTriMesh>>> LoadMesh(const GltfLoadParams& params);

    // Load and cache an image
    std::future<Result<std::shared_ptr<GpuImageProto>>> LoadImage(const ImageSource& source, const TexParams& params);

    // Clear cache
    void Clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Asset processor for offline asset conversion
 */
class AssetProcessor {
public:
    AssetProcessor();
    ~AssetProcessor();

    // Process mesh asset to cache directory
    Result<void> ProcessMeshAsset(const MeshAssetProcessParams& params);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Serialized asset reference (similar to AssetRef in Rust)
 */
template<typename T>
struct AssetRef {
    uint64_t identity;  // Hash of the asset

    AssetRef() : identity(0) {}
    explicit AssetRef(uint64_t id) : identity(id) {}

    bool operator==(const AssetRef& other) const { return identity == other.identity; }
    bool operator!=(const AssetRef& other) const { return identity != other.identity; }
    bool operator<(const AssetRef& other) const { return identity < other.identity; }
};

/**
 * @brief Flat vector for serialized data (similar to FlatVec in Rust)
 */
template<typename T>
struct FlatVec {
    uint64_t len;
    uint64_t offset;  // relative offset from this location

    const T* Data() const {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(&offset);
        return reinterpret_cast<const T*>(base + offset);
    }

    size_t Size() const { return static_cast<size_t>(len); }

    const T& operator[](size_t index) const {
        return Data()[index];
    }
};

/**
 * @brief Serialization context for flattening data structures
 */
class FlattenContext {
public:
    FlattenContext() = default;

    void WritePlainField(const void* data, size_t size);
    void WriteBytes(const void* data, size_t size);

    template<typename T>
    size_t WriteVecHeader(size_t count) {
        size_t fixupAddr = bytes_.size();
        WritePlainField(&count, sizeof(uint64_t));
        uint64_t offset = 0;
        WritePlainField(&offset, sizeof(uint64_t));
        return fixupAddr + sizeof(uint64_t);
    }

    void Finish(std::vector<uint8_t>& output);

    std::vector<uint8_t>& GetBytes() { return bytes_; }

private:
    struct DeferredBlob {
        size_t fixupAddr;
        std::unique_ptr<FlattenContext> nested;
    };

    std::vector<uint8_t> bytes_;
    std::vector<DeferredBlob> deferred_;
    std::optional<size_t> sectionIdx_;

    void AllocateSectionIndices();
    void AllocateSectionIndicesImpl(size_t& counter);
};

/**
 * @brief Serialized GPU image
 */
struct SerializedGpuImage {
    VkFormat format;
    std::array<uint32_t, 3> extent;
    FlatVec<FlatVec<uint8_t>> mips;
};

/**
 * @brief Serialized packed mesh
 */
struct SerializedPackedMesh {
    FlatVec<PackedVertex> verts;
    FlatVec<glm::vec2> uvs;
    FlatVec<glm::vec4> tangents;
    FlatVec<glm::vec4> colors;
    FlatVec<uint32_t> indices;
    FlatVec<uint32_t> materialIds;
    FlatVec<MeshMaterial> materials;
    FlatVec<AssetRef<SerializedGpuImage>> maps;
};

/**
 * @brief Serialize GPU image to bytes
 */
std::vector<uint8_t> SerializeGpuImage(const GpuImageProto& image);

/**
 * @brief Serialize packed mesh to bytes
 */
std::vector<uint8_t> SerializePackedMesh(const PackedTriMesh& mesh);

/**
 * @brief Deserialize GPU image from bytes
 */
const SerializedGpuImage* DeserializeGpuImage(const void* data);

/**
 * @brief Deserialize packed mesh from bytes
 */
const SerializedPackedMesh* DeserializePackedMesh(const void* data);

} // namespace tekki::asset

// Hash function for AssetRef
namespace std {
template<typename T>
struct hash<tekki::asset::AssetRef<T>> {
    size_t operator()(const tekki::asset::AssetRef<T>& ref) const {
        return std::hash<uint64_t>{}(ref.identity);
    }
};
}
