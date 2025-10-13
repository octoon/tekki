#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <string>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/resource_registry.h"
#include "tekki/render_graph/types.h"
#include "tekki/renderer/FrameConstantsLayout.h"

namespace tekki::render_graph {

// Forward declarations
class PassBuilder;
class CompiledRenderGraph;
class ExecutingRenderGraph;
class RetiredRenderGraph;

// Enum for synchronization type
enum class PassResourceAccessSyncType {
    AlwaysSync,
    SkipSyncIfSameAccessType
};

// Resource access type
struct PassResourceAccessType {
    vk_sync::AccessType AccessType;
    PassResourceAccessSyncType SyncType;

    PassResourceAccessType(vk_sync::AccessType accessType, PassResourceAccessSyncType syncType);
};

// Resource reference in a pass
struct PassResourceRef {
    GraphRawResourceHandle Handle;
    PassResourceAccessType Access;
};

// Recorded pass structure
struct RecordedPass {
    std::vector<PassResourceRef> Read;
    std::vector<PassResourceRef> Write;
    std::function<void(RenderPassApi*)> RenderFn;
    std::string Name;
    std::size_t Idx;

    RecordedPass(const std::string& name, std::size_t idx);
};

// Simple descriptor info for render graph
struct DescriptorInfo {
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;
    uint32_t binding;
    uint32_t count;
};

struct GraphResourceCreateInfo {
    GraphResourceDesc Desc;
};

// Import info for different resource types - matching Rust's enum with associated data
struct GraphResourceImportInfo {
    struct ImageImport {
        std::shared_ptr<Image> resource;
        vk_sync::AccessType access_type;
    };

    struct BufferImport {
        std::shared_ptr<Buffer> resource;
        vk_sync::AccessType access_type;
    };

    struct RayTracingAccelerationImport {
        std::shared_ptr<RayTracingAcceleration> resource;
        vk_sync::AccessType access_type;
    };

    struct SwapchainImage {};

    std::variant<ImageImport, BufferImport, RayTracingAccelerationImport, SwapchainImage> data;

    // Helper constructors
    static GraphResourceImportInfo Image(std::shared_ptr<class Image> resource, vk_sync::AccessType access_type) {
        GraphResourceImportInfo info;
        info.data = ImageImport{resource, access_type};
        return info;
    }

    static GraphResourceImportInfo Buffer(std::shared_ptr<class Buffer> resource, vk_sync::AccessType access_type) {
        GraphResourceImportInfo info;
        info.data = BufferImport{resource, access_type};
        return info;
    }

    static GraphResourceImportInfo RayTracingAcceleration(std::shared_ptr<class RayTracingAcceleration> resource, vk_sync::AccessType access_type) {
        GraphResourceImportInfo info;
        info.data = RayTracingAccelerationImport{resource, access_type};
        return info;
    }

    static GraphResourceImportInfo Swapchain() {
        GraphResourceImportInfo info;
        info.data = SwapchainImage{};
        return info;
    }
};

struct GraphResourceInfo {
    std::variant<GraphResourceCreateInfo, GraphResourceImportInfo> Info;
};

// Exportable graph resource - wraps handles for export
class ExportableGraphResource {
public:
    ExportableGraphResource(const Handle<Image>& handle)
        : type_(Type::Image), image_handle_(handle) {}
    ExportableGraphResource(const Handle<Buffer>& handle)
        : type_(Type::Buffer), buffer_handle_(handle) {}

    enum class Type {
        Image,
        Buffer
    };

    Type GetType() const { return type_; }
    const Handle<Image>& GetImageHandle() const { return image_handle_; }
    const Handle<Buffer>& GetBufferHandle() const { return buffer_handle_; }

    GraphRawResourceHandle GetRaw() const {
        return type_ == Type::Image ? image_handle_.raw : buffer_handle_.raw;
    }

private:
    Type type_;
    Handle<Image> image_handle_;
    Handle<Buffer> buffer_handle_;
};

struct RgComputePipelineHandle {
    std::size_t Id;
};

struct RgComputePipeline {
    ComputePipelineDesc Desc;
};

struct RgRasterPipelineHandle {
    std::size_t Id;
};

struct RgRasterPipeline {
    std::vector<PipelineShaderDesc> Shaders;
    RasterPipelineDesc Desc;
};

struct RgRtPipelineHandle {
    std::size_t Id;
};

struct RgRtPipeline {
    std::vector<PipelineShaderDesc> Shaders;
    RayTracingPipelineDesc Desc;
};

struct PredefinedDescriptorSet {
    std::unordered_map<std::uint32_t, DescriptorInfo> Bindings;
};

struct RenderDebugHook {
    std::string Name;
    std::uint64_t Id;
};

struct GraphDebugHook {
    RenderDebugHook RenderDebugHook;
};

// Pending debug pass structure
struct PendingDebugPass {
    Handle<Image> srcHandle;
};

// Forward declare for RenderGraph
struct ResourceLifetime {
    std::optional<std::size_t> LastAccess;
};

struct ResourceInfo {
    std::vector<ResourceLifetime> Lifetimes;
    std::vector<vk::ImageUsageFlags> ImageUsageFlags;
    std::vector<vk::BufferUsageFlags> BufferUsageFlags;
};

class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph() = default;

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = default;
    RenderGraph& operator=(RenderGraph&&) = default;

    template<typename Desc>
    Handle<typename Desc::Resource> Create(const Desc& desc);

    GraphRawResourceHandle CreateRawResource(const GraphResourceCreateInfo& info);

    template<typename Res>
    Handle<Res> Import(const std::shared_ptr<Res>& resource, vk_sync::AccessType accessTypeAtImportTime);

    template<typename Res>
    ExportedHandle<Res> Export(const Handle<Res>& resource, vk_sync::AccessType accessType);

    Handle<Image> GetSwapChain();

    PassBuilder AddPass(const std::string& name);

    CompiledRenderGraph Compile(PipelineCache* pipelineCache);

    void RecordPass(const RecordedPass& pass);

    std::vector<RecordedPass> Passes;
    std::vector<GraphResourceInfo> Resources;
    std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>> ExportedResources;
    std::vector<RgComputePipeline> ComputePipelines;
    std::vector<RgRasterPipeline> RasterPipelines;
    std::vector<RgRtPipeline> RtPipelines;
    std::unordered_map<std::uint32_t, PredefinedDescriptorSet> PredefinedDescriptorSetLayouts;
    std::optional<GraphDebugHook> DebugHook;
    std::optional<Handle<Image>> DebuggedResource;

private:
    ResourceInfo CalculateResourceInfo() const;
    std::optional<PendingDebugPass> HookDebugPass(const RecordedPass& pass);
};

struct RenderGraphExecutionParams {
    Device* Device;
    PipelineCache* PipelineCache;
    vk::DescriptorSet FrameDescriptorSet;
    tekki::renderer::FrameConstantsLayout FrameConstantsLayout;
    const VkProfilerData* ProfilerData;
};

struct RenderGraphPipelines {
    std::vector<ComputePipelineHandle> Compute;
    std::vector<RasterPipelineHandle> Raster;
    std::vector<RtPipelineHandle> Rt;
};

class CompiledRenderGraph {
public:
    CompiledRenderGraph(RenderGraph&& rg, const ResourceInfo& resourceInfo, RenderGraphPipelines&& pipelines);
    ~CompiledRenderGraph() = default;

    ExecutingRenderGraph BeginExecute(const RenderGraphExecutionParams& params, TransientResourceCache* transientResourceCache, DynamicConstants* dynamicConstants);

private:
    RenderGraph Rg;
    ResourceInfo ResourceInfo_;
    RenderGraphPipelines Pipelines;
};

class ExecutingRenderGraph {
public:
    ExecutingRenderGraph(std::deque<RecordedPass>&& passes, std::vector<GraphResourceInfo>&& resources, std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>>&& exportedResources, ResourceRegistry&& resourceRegistry);
    ~ExecutingRenderGraph() = default;

    void RecordMainCb(const CommandBuffer& cb);
    RetiredRenderGraph RecordPresentationCb(const CommandBuffer& cb, const std::shared_ptr<Image>& swapchainImage);

private:
    static void RecordPassCb(const RecordedPass& pass, ResourceRegistry* resourceRegistry, const CommandBuffer& cb);
    static void TransitionResource(Device* device, const CommandBuffer& cb, RegistryResource* resource, const PassResourceAccessType& access, bool debug, const std::string& dbgStr);

    std::deque<RecordedPass> Passes;
    std::vector<GraphResourceInfo> Resources;
    std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>> ExportedResources;
    ResourceRegistry ResourceRegistry_;
};

class RetiredRenderGraph {
public:
    RetiredRenderGraph(std::vector<RegistryResource>&& resources);
    ~RetiredRenderGraph() = default;

    template<typename Res>
    std::pair<const Res*, vk_sync::AccessType> ExportedResource(const ExportedHandle<Res>& handle) const;

    void ReleaseResources(TransientResourceCache* transientResourceCache);

private:
    std::vector<RegistryResource> Resources;
};

extern bool RGAllowPassOverlap;

vk::ImageUsageFlags ImageAccessMaskToUsageFlags(VkAccessFlags accessMask);
vk::BufferUsageFlags BufferAccessMaskToUsageFlags(VkAccessFlags accessMask);

}  // namespace tekki::render_graph