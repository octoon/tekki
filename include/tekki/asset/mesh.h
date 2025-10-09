#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tekki/backend/bytes.h>
#include <tekki/core/result.h>

namespace tekki::asset {

enum class TexGamma {
    Linear,
    Srgb
};

enum class TexCompressionMode {
    None,
    Rgba,
    Rg
};

struct TexParams {
    TexGamma Gamma;
    bool UseMips;
    TexCompressionMode Compression;
    std::optional<std::array<size_t, 4>> ChannelSwizzle;
};

class MeshMaterialMap {
public:
    enum class Type {
        Image,
        Placeholder
    };

    struct ImageData {
        // ImageSource Source; // TODO: Replace with actual ImageSource type
        TexParams Params;
    };

    MeshMaterialMap() = default;
    
    static MeshMaterialMap CreateImage(/*ImageSource source,*/ TexParams params) {
        MeshMaterialMap map;
        map.Type = Type::Image;
        // map.ImageData.Source = source;
        map.ImageData.Params = params;
        return map;
    }

    static MeshMaterialMap CreatePlaceholder(std::array<uint8_t, 4> values) {
        MeshMaterialMap map;
        map.Type = Type::Placeholder;
        map.PlaceholderValues = values;
        return map;
    }

    Type GetType() const { return Type; }
    const ImageData& GetImageData() const { return ImageData; }
    std::array<uint8_t, 4> GetPlaceholderValues() const { return PlaceholderValues; }

private:
    Type Type;
    ImageData ImageData;
    std::array<uint8_t, 4> PlaceholderValues;
};

struct MeshMaterialFlags {
    static constexpr uint32_t MeshMaterialFlagEmissiveUsedAsLight = 1;
};

#pragma pack(push, 1)
struct MeshMaterial {
    std::array<float, 4> BaseColorMult;
    std::array<uint32_t, 4> Maps;
    float RoughnessMult;
    float MetalnessFactor;
    std::array<float, 3> Emissive;
    uint32_t Flags;
    std::array<std::array<float, 6>, 4> MapTransforms;
};
#pragma pack(pop)

struct TriangleMesh {
    std::vector<std::array<float, 3>> Positions;
    std::vector<std::array<float, 3>> Normals;
    std::vector<std::array<float, 4>> Colors;
    std::vector<std::array<float, 2>> Uvs;
    std::vector<std::array<float, 4>> Tangents;
    std::vector<uint32_t> MaterialIds;
    std::vector<uint32_t> Indices;
    std::vector<MeshMaterial> Materials;
    std::vector<MeshMaterialMap> Maps;
    // std::vector<ImageSource> Images; // TODO: Replace with actual ImageSource type

    TriangleMesh() = default;
};

class LoadGltfScene {
public:
    std::string Path;
    float Scale;
    glm::quat Rotation;

    LoadGltfScene(std::string path, float scale, glm::quat rotation)
        : Path(std::move(path)), Scale(scale), Rotation(rotation) {}

    TriangleMesh Run();
};

#pragma pack(push, 1)
struct PackedVertex {
    std::array<float, 3> Pos;
    uint32_t Normal;
};
#pragma pack(pop)

uint32_t PackUnitDirection11_10_11(float x, float y, float z);

template<typename T>
class FlatVec {
public:
    uint64_t Len;
    uint64_t Offset;
    
    size_t GetLength() const { return static_cast<size_t>(Len); }
    bool IsEmpty() const { return Len == 0; }
    
    const T* GetData() const {
        return reinterpret_cast<const T*>(
            reinterpret_cast<const uint8_t*>(&Offset) + Offset);
    }
    
    const T& operator[](size_t index) const {
        return GetData()[index];
    }
    
    const T* begin() const { return GetData(); }
    const T* end() const { return GetData() + GetLength(); }
};

class FlattenCtx {
public:
    std::optional<size_t> SectionIdx;
    std::vector<uint8_t> Bytes;
    std::vector<struct DeferredBlob> Deferred;

    FlattenCtx() = default;
    
    void AllocateSectionIndices();
    void Finish(std::vector<uint8_t>& writer);
};

struct DeferredBlob {
    size_t FixupAddr;
    FlattenCtx Nested;
};

template<typename T>
struct AssetRef {
    uint64_t Identity;
    
    uint64_t GetIdentity() const { return Identity; }
    bool operator==(const AssetRef& other) const { return Identity == other.Identity; }
    bool operator!=(const AssetRef& other) const { return Identity != other.Identity; }
    bool operator<(const AssetRef& other) const { return Identity < other.Identity; }
};

namespace GpuImage {
    struct Proto {
        // kajiya_backend::ash::vk::Format Format; // TODO: Replace with actual format type
        std::array<uint32_t, 3> Extent;
        std::vector<std::vector<uint8_t>> Mips;
        
        void FlattenInto(std::vector<uint8_t>& writer);
    };

    struct Flat {
        // kajiya_backend::ash::vk::Format Format;
        std::array<uint32_t, 3> Extent;
        FlatVec<FlatVec<uint8_t>> Mips;
    };
}

namespace PackedTriMesh {
    struct Proto {
        std::vector<PackedVertex> Verts;
        std::vector<std::array<float, 2>> Uvs;
        std::vector<std::array<float, 4>> Tangents;
        std::vector<std::array<float, 4>> Colors;
        std::vector<uint32_t> Indices;
        std::vector<uint32_t> MaterialIds;
        std::vector<MeshMaterial> Materials;
        std::vector<AssetRef<GpuImage::Flat>> Maps;
        
        void FlattenInto(std::vector<uint8_t>& writer);
    };

    struct Flat {
        FlatVec<PackedVertex> Verts;
        FlatVec<std::array<float, 2>> Uvs;
        FlatVec<std::array<float, 4>> Tangents;
        FlatVec<std::array<float, 4>> Colors;
        FlatVec<uint32_t> Indices;
        FlatVec<uint32_t> MaterialIds;
        FlatVec<MeshMaterial> Materials;
        FlatVec<AssetRef<GpuImage::Flat>> Maps;
    };
}

using PackedTriangleMesh = PackedTriMesh::Proto;

PackedTriangleMesh PackTriangleMesh(const TriangleMesh& mesh);

#pragma pack(push, 1)
struct GpuMaterial {
    std::array<float, 4> BaseColorMult;
    std::array<uint32_t, 4> Maps;
    
    GpuMaterial() : BaseColorMult{0.0f}, Maps{0} {}
};
#pragma pack(pop)

struct TangentCalcContext {
    const std::vector<uint32_t>& Indices;
    const std::vector<std::array<float, 3>>& Positions;
    const std::vector<std::array<float, 3>>& Normals;
    const std::vector<std::array<float, 2>>& Uvs;
    std::vector<std::array<float, 4>>& Tangents;
    
    void GenerateTangents();
};

} // namespace tekki::asset