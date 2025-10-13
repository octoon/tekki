#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>
#include <mutex>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/dynamic_constants.h"
#include "tekki/rust_shaders_shared/view_constants.h"
#include "tekki/rust_shaders_shared/frame_constants.h"
#include "tekki/rust_shaders_shared/camera.h"
#include "tekki/rust_shaders_shared/render_overrides.h"
#include "tekki/asset/mesh.h"
#include "tekki/core/result.h"

namespace tekki::renderer {

// Forward declarations for renderer subsystems
class PostProcessRenderer;
class SsgiRenderer;
class RtrRenderer;
class LightingRenderer;
class IrcacheRenderer;
class RtdgiRenderer;
class TaaRenderer;
class ShadowDenoiseRenderer;
class IblRenderer;

constexpr bool USE_TAA_JITTER = true;

struct GpuMesh {
    uint32_t VertexCoreOffset;
    uint32_t VertexUvOffset;
    uint32_t VertexMatOffset;
    uint32_t VertexAuxOffset;
    uint32_t VertexTangentOffset;
    uint32_t MatDataOffset;
    uint32_t IndexOffset;
};

class MeshHandle {
public:
    explicit MeshHandle(size_t value) : Value(value) {}
    size_t GetValue() const { return Value; }
    
private:
    size_t Value;
};

class InstanceHandle {
public:
    explicit InstanceHandle(size_t value) : Value(value) {}
    static const InstanceHandle Invalid;
    
    bool IsValid() const { return *this != Invalid; }
    size_t GetValue() const { return Value; }
    
    bool operator==(const InstanceHandle& other) const { return Value == other.Value; }
    bool operator!=(const InstanceHandle& other) const { return Value != other.Value; }
    
private:
    size_t Value;
};

constexpr size_t MAX_GPU_MESHES = 1024;
constexpr size_t VERTEX_BUFFER_CAPACITY = 1024 * 1024 * 1024;
constexpr size_t TLAS_PREALLOCATE_BYTES = 1024 * 1024 * 32;

struct InstanceDynamicParameters {
    float EmissiveMultiplier;
    
    InstanceDynamicParameters() : EmissiveMultiplier(1.0f) {}
};

struct MeshInstance {
    glm::mat4 Transform;
    glm::mat4 PrevTransform;
    MeshHandle Mesh;
    InstanceDynamicParameters DynamicParameters;
};

enum class RenderDebugMode {
    None,
    WorldRadianceCache
};

struct TriangleLight {
    std::array<std::array<float, 3>, 3> Verts;
    std::array<float, 3> Radiance;
    
    TriangleLight Transform(const glm::vec3& translation, const glm::mat3& rotation) const {
        TriangleLight result = *this;
        auto v0 = rotation * glm::vec3(Verts[0][0], Verts[0][1], Verts[0][2]) + translation;
        auto v1 = rotation * glm::vec3(Verts[1][0], Verts[1][1], Verts[1][2]) + translation;
        auto v2 = rotation * glm::vec3(Verts[2][0], Verts[2][1], Verts[2][2]) + translation;
        result.Verts[0] = { v0.x, v0.y, v0.z };
        result.Verts[1] = { v1.x, v1.y, v1.z };
        result.Verts[2] = { v2.x, v2.y, v2.z };
        return result;
    }
    
    TriangleLight ScaleRadiance(const glm::vec3& scale) const {
        TriangleLight result = *this;
        result.Radiance = { Radiance[0] * scale.x, Radiance[1] * scale.y, Radiance[2] * scale.z };
        return result;
    }
};

struct MeshLightSet {
    std::vector<TriangleLight> Lights;
};

struct UploadedTriMesh {
    uint64_t IndexBufferOffset;
    uint32_t IndexCount;
};

struct HistogramClipping {
    float Low;
    float High;
    
    HistogramClipping() : Low(0.0f), High(0.0f) {}
};

struct DynamicExposureState {
    bool Enabled;
    float SpeedLog2;
    HistogramClipping HistogramClipping;
    float EvFast;
    float EvSlow;
    
    DynamicExposureState() : Enabled(false), SpeedLog2(0.0f), EvFast(0.0f), EvSlow(0.0f) {}
    
    float EvSmoothed() const;
    void Update(float ev, float dt);
};

struct ExposureState {
    float PreMult;
    float PostMult;
    float PreMultPrev;
    float PreMultDelta;
    
    ExposureState() : PreMult(1.0f), PostMult(1.0f), PreMultPrev(1.0f), PreMultDelta(1.0f) {}
};

enum class RenderMode {
    Standard = 0,
    Reference = 1
};

class BindlessImageHandle {
public:
    explicit BindlessImageHandle(uint32_t value) : Value(value) {}
    uint32_t GetValue() const { return Value; }
    
private:
    uint32_t Value;
};

struct AddMeshOptions {
    bool UseLights;

    AddMeshOptions() : UseLights(false) {}

    AddMeshOptions WithLights(bool value) {
        AddMeshOptions result = *this;
        result.UseLights = value;
        return result;
    }
};

struct WorldFrameDesc {
    tekki::rust_shaders_shared::CameraMatrices CameraMatrices;
    glm::vec2 RenderExtent;
    glm::vec3 SunDirection;
};

class WorldRenderer {
public:
    static std::shared_ptr<WorldRenderer> CreateEmpty(
        const glm::uvec2& renderExtent,
        const glm::uvec2& temporalUpscaleExtent,
        const std::shared_ptr<tekki::backend::vulkan::Device>& device);

    void AddImageLut(std::unique_ptr<class ComputeImageLut> computer, size_t id);
    BindlessImageHandle AddImage(const std::shared_ptr<tekki::backend::vulkan::Image>& image);
    MeshHandle AddMesh(const std::shared_ptr<tekki::asset::PackedTriangleMesh>& mesh, const AddMeshOptions& opts);
    InstanceHandle AddInstance(MeshHandle mesh, const glm::mat4& transform);
    void RemoveInstance(InstanceHandle inst);
    void SetInstanceTransform(InstanceHandle inst, const glm::mat4& transform);
    const InstanceDynamicParameters& GetInstanceDynamicParameters(InstanceHandle inst) const;
    InstanceDynamicParameters& GetInstanceDynamicParametersMut(InstanceHandle inst);
    void BuildRayTracingTopLevelAcceleration();
    void ResetFrameIdx();
    void PrepareTopLevelAcceleration(class TemporalRenderGraph& rg);
    ExposureState GetExposureState() const;
    void PrepareRenderGraph(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc);
    struct FrameConstantsLayout PrepareFrameConstants(
        tekki::backend::DynamicConstants& dynamicConstants,
        const WorldFrameDesc& frameDesc,
        float deltaTimeSeconds);
    void RetireFrame();

private:
    WorldRenderer() = default;
    
    void WriteDescriptorSetBuffer(
        VkDevice device,
        VkDescriptorSet set,
        uint32_t dstBinding,
        const std::shared_ptr<tekki::backend::vulkan::Buffer>& buffer);
    BindlessImageHandle AddBindlessImageView(VkImageView view);
    void StorePrevMeshTransforms();
    void UpdatePreExposure();
    class ImageHandle PrepareRenderGraphStandard(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc);
    class ImageHandle PrepareRenderGraphReference(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc);
    
    std::shared_ptr<tekki::backend::vulkan::Device> Device;
    std::shared_ptr<class RenderPass> RasterSimpleRenderPass;
    VkDescriptorSet BindlessDescriptorSet;
    std::vector<UploadedTriMesh> Meshes;
    std::vector<MeshLightSet> MeshLights;
    std::vector<MeshInstance> Instances;
    std::vector<InstanceHandle> InstanceHandles;
    std::unordered_map<InstanceHandle, size_t> InstanceHandleToIndex;
    std::mutex VertexBufferMutex;
    std::shared_ptr<tekki::backend::vulkan::Buffer> VertexBuffer;
    uint64_t VertexBufferWritten;
    std::mutex MeshBufferMutex;
    std::shared_ptr<tekki::backend::vulkan::Buffer> MeshBuffer;
    std::vector<std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>> MeshBlas;
    std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration> Tlas;
    tekki::backend::vulkan::RayTracingAccelerationScratchBuffer AccelScratch;
    std::vector<std::shared_ptr<tekki::backend::vulkan::Image>> BindlessImages;
    size_t NextBindlessImageId;
    size_t NextInstanceHandle;
    tekki::backend::vulkan::Buffer BindlessTextureSizes;
    std::vector<class ImageLut> ImageLuts;
    uint32_t FrameIdx;
    std::optional<tekki::rust_shaders_shared::CameraMatrices> PrevCameraMatrices;
    glm::uvec2 TemporalUpscaleExtent;
    std::vector<glm::vec2> SupersampleOffsets;
    std::shared_ptr<class GraphDebugHook> RgDebugHook;
    RenderMode CurrentRenderMode;
    bool ResetReferenceAccumulation;
    std::unique_ptr<PostProcessRenderer> Post;
    std::unique_ptr<SsgiRenderer> Ssgi;
    std::unique_ptr<RtrRenderer> Rtr;
    std::unique_ptr<LightingRenderer> Lighting;
    std::unique_ptr<IrcacheRenderer> Ircache;
    std::unique_ptr<RtdgiRenderer> Rtdgi;
    std::unique_ptr<TaaRenderer> Taa;
    std::unique_ptr<ShadowDenoiseRenderer> ShadowDenoise;
    std::unique_ptr<IblRenderer> Ibl;
    bool UseDlss;
    RenderDebugMode DebugMode;
    size_t DebugShadingMode;
    bool DebugShowWrc;
    float EvShift;
    DynamicExposureState DynamicExposure;
    float Contrast;
    float SunSizeMultiplier;
    glm::vec3 SunColorMultiplier;
    glm::vec3 SkyAmbient;
    tekki::rust_shaders_shared::RenderOverrides RenderOverrides;
    std::array<ExposureState, 2> ExposureStates;
};

inline float RadicalInverse(uint32_t n, uint32_t base) {
    float val = 0.0f;
    float invBase = 1.0f / static_cast<float>(base);
    float invBi = invBase;
    
    while (n > 0) {
        uint32_t d_i = n % base;
        val += static_cast<float>(d_i) * invBi;
        n = static_cast<uint32_t>(static_cast<float>(n) * invBase);
        invBi *= invBase;
    }
    
    return val;
}

}