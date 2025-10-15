#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <filesystem>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vk_sync.h"

// Include needed types from render_graph
#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/buffer.h"

namespace tekki::render_graph {

// Use Device from the included header
using Device = backend::vulkan::Device;

// Ref template is defined in resource.h, forward declare here
template<typename Res, typename ViewType>
struct Ref;

// Resource descriptor types - variant of Image or Buffer descriptor
using GraphResourceDesc = std::variant<ImageDesc, BufferDesc>;

struct DescriptorSetLayoutOpts {
    struct Builder {
        Builder& Replace([[maybe_unused]] const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
            // Placeholder
            return *this;
        }
        DescriptorSetLayoutOpts Build() { return DescriptorSetLayoutOpts{}; }
    };
};

// Forward declare for Builder
struct ComputePipelineDesc;

struct ComputePipelineDescBuilder {
    ComputePipelineDesc* desc;

    ComputePipelineDescBuilder(ComputePipelineDesc* d) : desc(d) {}
    ComputePipelineDescBuilder& ComputeHlsl(const std::filesystem::path& path);
    ComputePipelineDesc Build();
};

struct ComputePipelineDesc {
    // Placeholder for compute pipeline descriptor
    std::string shader_entry_point;
    VkPipelineLayout layout;
    std::unordered_map<uint32_t, std::pair<uint32_t, DescriptorSetLayoutOpts>> descriptor_set_opts;

    static ComputePipelineDescBuilder Builder() {
        static thread_local ComputePipelineDesc desc;
        return ComputePipelineDescBuilder(&desc);
    }
};

struct PipelineShaderDesc {
    // Placeholder for pipeline shader descriptor
    std::string shader_module;
    std::string entry_point;
    VkShaderStageFlagBits stage;
};

struct RasterPipelineDesc {
    // Placeholder for raster pipeline descriptor
    VkPipelineLayout layout;
    VkRenderPass render_pass;
    std::unordered_map<uint32_t, std::pair<uint32_t, DescriptorSetLayoutOpts>> descriptor_set_opts;
};

struct RayTracingPipelineDesc {
    // Placeholder for ray tracing pipeline descriptor
    VkPipelineLayout layout;
    std::unordered_map<uint32_t, std::pair<uint32_t, DescriptorSetLayoutOpts>> descriptor_set_opts;
};

// Handle types
struct GraphRawResourceHandle {
    uint32_t id = 0;
    uint32_t version = 0;

    bool is_valid() const { return id != 0; }
    bool operator==(const GraphRawResourceHandle& other) const { return id == other.id && version == other.version; }

    GraphRawResourceHandle next_version() const {
        return GraphRawResourceHandle{id, version + 1};
    }
};

struct ComputePipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

struct RasterPipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

struct RtPipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

// Exported handle
template<typename T>
struct ExportedHandle {
    uint64_t id = 0;

    bool is_valid() const { return id != 0; }
    bool operator==(const ExportedHandle& other) const { return id == other.id; }
};

// Pipeline cache
class PipelineCache {
public:
    // Placeholder implementation
    ComputePipelineHandle RegisterCompute([[maybe_unused]] const ComputePipelineDesc& desc) { return ComputePipelineHandle{}; }
    RasterPipelineHandle RegisterRaster([[maybe_unused]] const std::vector<PipelineShaderDesc>& shaders, [[maybe_unused]] const RasterPipelineDesc& desc) { return RasterPipelineHandle{}; }
    RtPipelineHandle RegisterRayTracing([[maybe_unused]] const std::vector<PipelineShaderDesc>& shaders, [[maybe_unused]] const RayTracingPipelineDesc& desc) { return RtPipelineHandle{}; }
};

// Transient resource cache
class TransientResourceCache {
public:
    // Placeholder implementation
    void ReleaseResources() {}
    std::shared_ptr<Image> GetImage([[maybe_unused]] const ImageDesc& desc) { return nullptr; }
    std::shared_ptr<Buffer> GetBuffer([[maybe_unused]] const BufferDesc& desc) { return nullptr; }
    void InsertImage([[maybe_unused]] std::shared_ptr<Image> image) {}
    void InsertBuffer([[maybe_unused]] std::shared_ptr<Buffer> buffer) {}
};

// Dynamic constants
class DynamicConstants {
public:
    // Placeholder implementation
    void Update() {}
};

// Command buffer
class CommandBuffer {
public:
    VkCommandBuffer Raw;

    CommandBuffer() : Raw(VK_NULL_HANDLE) {}
    explicit CommandBuffer(VkCommandBuffer cmd) : Raw(cmd) {}

    void Begin() {}
    void End() {}
};

// Forward declarations for variant types - RayTracingAcceleration only
class RayTracingAcceleration;
struct GraphResourceInfo;

// RayTracingAcceleration descriptor and class
struct RayTracingAccelerationDesc {
    using Resource = RayTracingAcceleration;
    // Placeholder - add actual fields as needed
    uint64_t size = 0;
};

class RayTracingAcceleration {
public:
    using Desc = RayTracingAccelerationDesc;

    // Placeholder implementation
    RayTracingAccelerationDesc desc;

    RayTracingAcceleration() = default;
    explicit RayTracingAcceleration(const RayTracingAccelerationDesc& d) : desc(d) {}
};

// Forward declare before use
struct GraphResourceInfo;

// Pending render resource info
struct PendingRenderResourceInfo {
    std::shared_ptr<GraphResourceInfo> Resource;
};

// Any render resource variant - use wrapper structs to make types distinguishable
namespace AnyRenderResource {
    struct OwnedImage {
        std::shared_ptr<Image> resource;
        OwnedImage() = default;
        explicit OwnedImage(std::shared_ptr<Image> res) : resource(std::move(res)) {}
    };

    struct ImportedImage {
        std::shared_ptr<Image> resource;
        ImportedImage() = default;
        explicit ImportedImage(std::shared_ptr<Image> res) : resource(std::move(res)) {}
    };

    struct OwnedBuffer {
        std::shared_ptr<Buffer> resource;
        OwnedBuffer() = default;
        explicit OwnedBuffer(std::shared_ptr<Buffer> res) : resource(std::move(res)) {}
    };

    struct ImportedBuffer {
        std::shared_ptr<Buffer> resource;
        ImportedBuffer() = default;
        explicit ImportedBuffer(std::shared_ptr<Buffer> res) : resource(std::move(res)) {}
    };

    struct ImportedRayTracingAcceleration {
        std::shared_ptr<RayTracingAcceleration> resource;
        ImportedRayTracingAcceleration() = default;
        explicit ImportedRayTracingAcceleration(std::shared_ptr<RayTracingAcceleration> res) : resource(std::move(res)) {}
    };

    using Pending = PendingRenderResourceInfo;
}

// Registry resource with access tracking
struct RegistryResource {
    std::variant<
        AnyRenderResource::OwnedImage,
        AnyRenderResource::ImportedImage,
        AnyRenderResource::OwnedBuffer,
        AnyRenderResource::ImportedBuffer,
        AnyRenderResource::ImportedRayTracingAcceleration,
        AnyRenderResource::Pending
    > Resource;
    vk_sync::AccessType AccessType;
};

// Forward declare execution params
struct RenderGraphExecutionParams;
struct RenderGraphPipelines;

// Resource registry
class ResourceRegistry {
public:
    const RenderGraphExecutionParams* ExecutionParams;
    std::vector<RegistryResource> Resources;
    DynamicConstants* DynamicConstants_;
    const RenderGraphPipelines* Pipelines;

    ResourceRegistry(
        const RenderGraphExecutionParams* params,
        std::vector<RegistryResource>&& resources,
        DynamicConstants* dynamicConstants,
        const RenderGraphPipelines* pipelines);

    void RegisterResource([[maybe_unused]] const GraphRawResourceHandle& handle, [[maybe_unused]] const std::shared_ptr<void>& resource) {}
    std::shared_ptr<void> GetResource([[maybe_unused]] const GraphRawResourceHandle& handle) const { return nullptr; }

    // Image access helpers - matching Rust's naming convention
    template<typename ViewType>
    const Image& image(const Ref<Image, ViewType>& ref) const {
        return image_from_raw_handle<ViewType>(ref.handle);
    }

    template<typename ViewType>
    const Image& image_from_raw_handle(GraphRawResourceHandle handle) const;

    // Buffer access helpers
    template<typename ViewType>
    const Buffer& buffer(const Ref<Buffer, ViewType>& ref) const {
        return buffer_from_raw_handle<ViewType>(ref.handle);
    }

    template<typename ViewType>
    const Buffer& buffer_from_raw_handle(GraphRawResourceHandle handle) const;
};

// Render pass API
class RenderPassApi {
public:
    CommandBuffer cb;
    ResourceRegistry* registry;

    RenderPassApi(const CommandBuffer& commandBuffer, ResourceRegistry* reg)
        : cb(commandBuffer), registry(reg) {}

    const Device& Device() const;
    ResourceRegistry& Resources() { return *registry; }
    const ResourceRegistry& Resources() const { return *registry; }

    void SetPipeline([[maybe_unused]] const ComputePipelineHandle& pipeline) {}
    void SetPipeline([[maybe_unused]] const RasterPipelineHandle& pipeline) {}
    void SetPipeline([[maybe_unused]] const RtPipelineHandle& pipeline) {}
    void Dispatch([[maybe_unused]] uint32_t x, [[maybe_unused]] uint32_t y, [[maybe_unused]] uint32_t z) {}
    void Draw([[maybe_unused]] uint32_t vertex_count, [[maybe_unused]] uint32_t instance_count, [[maybe_unused]] uint32_t first_vertex, [[maybe_unused]] uint32_t first_instance) {}
};

// Forward declaration of profiler scope
namespace kajiya_backend {
namespace gpu_profiler {
class ScopeId {};
}
}

// Profiler data
struct VkProfilerData {
    // Placeholder
    uint64_t timestamp;

    // Scope management
    using ScopeHandle = uint64_t;
    ScopeHandle BeginScope([[maybe_unused]] VkDevice device, [[maybe_unused]] VkCommandBuffer cmd, [[maybe_unused]] const kajiya_backend::gpu_profiler::ScopeId& scopeId) {
        return 0;
    }
    void EndScope([[maybe_unused]] VkDevice device, [[maybe_unused]] VkCommandBuffer cmd, [[maybe_unused]] ScopeHandle scope) {}
};

// GPU view types
struct GpuUav {};
struct GpuSrv {};
struct GpuRt {};

// Raster pipeline descriptor builder
struct RasterPipelineDescBuilder {
    RasterPipelineDesc desc;

    // Placeholder implementation
    RasterPipelineDescBuilder() = default;

    RasterPipelineDesc Build() { return std::move(desc); }
};

} // namespace tekki::render_graph

// Inline implementations
inline tekki::render_graph::ComputePipelineDescBuilder& tekki::render_graph::ComputePipelineDescBuilder::ComputeHlsl(const std::filesystem::path& path) {
    desc->shader_entry_point = path.string();
    return *this;
}

inline tekki::render_graph::ComputePipelineDesc tekki::render_graph::ComputePipelineDescBuilder::Build() {
    return std::move(*desc);
}