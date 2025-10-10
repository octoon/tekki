#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <string>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/resource_registry.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/renderer/FrameConstantsLayout.h"

namespace tekki::render_graph {

struct GraphResourceCreateInfo {
    GraphResourceDesc Desc;
};

enum class GraphResourceImportInfo {
    Image,
    Buffer,
    RayTracingAcceleration,
    SwapchainImage
};

struct GraphResourceInfo {
    std::variant<GraphResourceCreateInfo, GraphResourceImportInfo> Info;
};

enum class ExportableGraphResource {
    Image,
    Buffer
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
    std::unordered_map<std::uint32_t, tekki::backend::vulkan::DescriptorInfo> Bindings;
};

struct RenderDebugHook {
    std::string Name;
    std::uint64_t Id;
};

struct GraphDebugHook {
    RenderDebugHook RenderDebugHook;
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

struct ResourceLifetime {
    std::optional<std::size_t> LastAccess;
};

struct ResourceInfo {
    std::vector<ResourceLifetime> Lifetimes;
    std::vector<vk::ImageUsageFlags> ImageUsageFlags;
    std::vector<vk::BufferUsageFlags> BufferUsageFlags;
};

struct RenderGraphExecutionParams {
    const Device* Device;
    PipelineCache* PipelineCache;
    vk::DescriptorSet FrameDescriptorSet;
    FrameConstantsLayout FrameConstantsLayout;
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
    static void TransitionResource(const Device* device, const CommandBuffer& cb, RegistryResource* resource, const PassResourceAccessType& access, bool debug, const std::string& dbgStr);

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

enum class PassResourceAccessSyncType {
    AlwaysSync,
    SkipSyncIfSameAccessType
};

struct PassResourceAccessType {
    vk_sync::AccessType AccessType;
    PassResourceAccessSyncType SyncType;

    PassResourceAccessType(vk_sync::AccessType accessType, PassResourceAccessSyncType syncType);
};

struct PassResourceRef {
    GraphRawResourceHandle Handle;
    PassResourceAccessType Access;
};

struct RecordedPass {
    std::vector<PassResourceRef> Read;
    std::vector<PassResourceRef> Write;
    std::function<void(RenderPassApi*)> RenderFn;
    std::string Name;
    std::size_t Idx;

    RecordedPass(const std::string& name, std::size_t idx);
};

extern bool RGAllowPassOverlap;

vk::ImageUsageFlags ImageAccessMaskToUsageFlags(vk::AccessFlags accessMask);
vk::BufferUsageFlags BufferAccessMaskToUsageFlags(vk::AccessFlags accessMask);

}