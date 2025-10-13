#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/backend/dynamic_constants.h"
#include "device.h"
#include "shader.h"
#include "buffer.h"

namespace tekki::backend::vulkan {

constexpr size_t RT_TLAS_SCRATCH_BUFFER_SIZE = 256 * 1024;

enum class RayTracingGeometryType {
    Triangle = 0,
    BoundingBox = 1
};

struct RayTracingGeometryPart {
    size_t IndexCount;
    size_t IndexOffset;
    uint32_t MaxVertex;
};

struct RayTracingGeometryDesc {
    RayTracingGeometryType GeometryType;
    VkDeviceAddress VertexBuffer;
    VkDeviceAddress IndexBuffer;
    VkFormat VertexFormat;
    size_t VertexStride;
    std::vector<RayTracingGeometryPart> Parts;
};

struct RayTracingInstanceDesc {
    std::shared_ptr<RayTracingAcceleration> Blas;
    glm::mat4 Transformation;
    uint32_t MeshIndex;
};

struct RayTracingTopAccelerationDesc {
    std::vector<RayTracingInstanceDesc> Instances;
    size_t PreallocateBytes;
};

struct RayTracingBottomAccelerationDesc {
    std::vector<RayTracingGeometryDesc> Geometries;
};

struct RayTracingShaderTableDesc {
    uint32_t RaygenEntryCount;
    uint32_t HitEntryCount;
    uint32_t MissEntryCount;
};

class RayTracingAcceleration {
public:
    // Descriptor type - can be either top-level or bottom-level
    using Desc = std::variant<RayTracingTopAccelerationDesc, RayTracingBottomAccelerationDesc>;

    VkAccelerationStructureKHR Raw;
    Buffer BackingBuffer;

    friend class Device;
};

class RayTracingAccelerationScratchBuffer {
public:
    std::shared_ptr<std::mutex> BufferMutex;
    Buffer Buffer;

    friend class Device;
};

class RayTracingShaderTable {
public:
    std::shared_ptr<Buffer> RaygenShaderBindingTableBuffer;
    VkStridedDeviceAddressRegionKHR RaygenShaderBindingTable;
    std::shared_ptr<Buffer> MissShaderBindingTableBuffer;
    VkStridedDeviceAddressRegionKHR MissShaderBindingTable;
    std::shared_ptr<Buffer> HitShaderBindingTableBuffer;
    VkStridedDeviceAddressRegionKHR HitShaderBindingTable;
    std::shared_ptr<Buffer> CallableShaderBindingTableBuffer;
    VkStridedDeviceAddressRegionKHR CallableShaderBindingTable;
};

class RayTracingPipeline {
public:
    ShaderPipelineCommon Common;
    RayTracingShaderTable Sbt;
    
    const ShaderPipelineCommon& operator*() const { return Common; }
    const ShaderPipelineCommon* operator->() const { return &Common; }
};

struct RayTracingPipelineDesc {
    std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> DescriptorSetOpts;
    uint32_t MaxPipelineRayRecursionDepth;

    RayTracingPipelineDesc() : MaxPipelineRayRecursionDepth(1) {}

    RayTracingPipelineDesc& WithMaxPipelineRayRecursionDepth(uint32_t depth) {
        MaxPipelineRayRecursionDepth = depth;
        return *this;
    }
};

#pragma pack(push, 1)
struct GeometryInstance {
    float Transform[12];
    uint32_t InstanceIdAndMask;
    uint32_t InstanceSbtOffsetAndFlags;
    VkDeviceAddress BlasAddress;
    
    GeometryInstance() = default;
    
    GeometryInstance(const float transform[12], uint32_t id, uint8_t mask, uint32_t sbtOffset, VkGeometryInstanceFlagsKHR flags, VkDeviceAddress blasAddress) {
        std::copy(transform, transform + 12, Transform);
        BlasAddress = blasAddress;
        SetId(id);
        SetMask(mask);
        SetSbtOffset(sbtOffset);
        SetFlags(flags);
    }
    
    void SetId(uint32_t id) {
        id = id & 0x00ffffff;
        InstanceIdAndMask |= id;
    }
    
    void SetMask(uint8_t mask) {
        uint32_t maskVal = static_cast<uint32_t>(mask);
        InstanceIdAndMask |= maskVal << 24;
    }
    
    void SetSbtOffset(uint32_t offset) {
        offset = offset & 0x00ffffff;
        InstanceSbtOffsetAndFlags |= offset;
    }
    
    void SetFlags(VkGeometryInstanceFlagsKHR flags) {
        InstanceSbtOffsetAndFlags |= static_cast<uint32_t>(flags) << 24;
    }
};
#pragma pack(pop)


std::shared_ptr<RayTracingPipeline> CreateRayTracingPipeline(const std::shared_ptr<Device>& device, const std::vector<PipelineShader<std::vector<uint32_t>>>& shaders, const RayTracingPipelineDesc& desc);

} // namespace tekki::backend::vulkan