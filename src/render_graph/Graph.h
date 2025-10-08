#pragma once

#include "Resource.h"
#include "ResourceRegistry.h"
#include "../backend/vulkan/device.h"
#include "../backend/vulkan/shader.h"
#include "../backend/vulkan/transient_resource_cache.h"
#include "../backend/vulkan/dynamic_constants.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace tekki::render_graph {

// Forward declarations
class PassBuilder;
struct RecordedPass;

// Resource creation info
struct GraphResourceCreateInfo {
    GraphResourceDesc desc;
};

// Resource import info
struct GraphResourceImportInfo {
    enum class Type {
        Image,
        Buffer,
        RayTracingAcceleration,
        SwapchainImage
    };

    Type type;
    std::variant<
        std::shared_ptr<Image>,
        std::shared_ptr<Buffer>,
        std::shared_ptr<RayTracingAcceleration>
    > resource;
    vk_sync::AccessType access_type;
};

// Resource info
struct GraphResourceInfo {
    enum class Type {
        Created,
        Imported
    };

    Type type;
    std::variant<GraphResourceCreateInfo, GraphResourceImportInfo> data;
};

// Exportable resource
struct ExportableGraphResource {
    enum class Type {
        Image,
        Buffer
    };

    Type type;
    std::variant<Handle<ImageResource>, Handle<BufferResource>> data;

    GraphRawResourceHandle raw() const {
        switch (type) {
            case Type::Image:
                return std::get<Handle<ImageResource>>(data).raw;
            case Type::Buffer:
                return std::get<Handle<BufferResource>>(data).raw;
        }
    }
};

// Pipeline handles
struct RgComputePipelineHandle {
    size_t id;
};

struct RgComputePipeline {
    ComputePipelineDesc desc;
};

struct RgRasterPipelineHandle {
    size_t id;
};

struct RgRasterPipeline {
    std::vector<PipelineShaderDesc> shaders;
    RasterPipelineDesc desc;
};

struct RgRtPipelineHandle {
    size_t id;
};

struct RgRtPipeline {
    std::vector<PipelineShaderDesc> shaders;
    RayTracingPipelineDesc desc;
};

// Predefined descriptor set
struct PredefinedDescriptorSet {
    std::unordered_map<uint32_t, rspirv_reflect::DescriptorInfo> bindings;
};

// Debug hooks
struct RenderDebugHook {
    std::string name;
    uint64_t id;
};

struct GraphDebugHook {
    RenderDebugHook render_debug_hook;
};

// Main render graph class
class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph() = default;

    // Resource creation
    template<typename Desc>
    Handle<typename ResourceDescTraits<Desc>::ResourceType> create(const Desc& desc);

    // Resource import/export
    template<typename Res>
    Handle<Res> import(std::shared_ptr<Res> resource, vk_sync::AccessType access_type);

    template<typename Res>
    ExportedHandle<Res> export(Handle<Res> resource, vk_sync::AccessType access_type);

    // Pass management
    PassBuilder add_pass(const std::string& name);

    // Compilation
    CompiledRenderGraph compile(PipelineCache* pipeline_cache);

    // Swapchain
    Handle<ImageResource> get_swap_chain();

private:
    std::vector<RecordedPass> passes;
    std::vector<GraphResourceInfo> resources;
    std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>> exported_resources;
    std::vector<RgComputePipeline> compute_pipelines;
    std::vector<RgRasterPipeline> raster_pipelines;
    std::vector<RgRtPipeline> rt_pipelines;
    std::unordered_map<uint32_t, PredefinedDescriptorSet> predefined_descriptor_set_layouts;
    std::optional<GraphDebugHook> debug_hook;
    std::optional<Handle<ImageResource>> debugged_resource;

    GraphRawResourceHandle create_raw_resource(const GraphResourceCreateInfo& info);
    void record_pass(RecordedPass pass);
    ResourceInfo calculate_resource_info() const;
    std::optional<PendingDebugPass> hook_debug_pass(const RecordedPass& pass);

    friend class PassBuilder;
};

// Import/export trait
template<typename Res>
class ImportExportToRenderGraph {
public:
    static Handle<Res> import(std::shared_ptr<Res> self, RenderGraph& rg, vk_sync::AccessType access_type);
    static ExportedHandle<Res> export(Handle<Res> resource, RenderGraph& rg, vk_sync::AccessType access_type);
};

// Type equality trait
template<typename T>
struct TypeEquals {
    using Other = T;
    static Other same(T value) { return value; }
};

// Execution parameters
struct RenderGraphExecutionParams {
    Device* device;
    PipelineCache* pipeline_cache;
    vk::DescriptorSet frame_descriptor_set;
    FrameConstantsLayout frame_constants_layout;
    VkProfilerData* profiler_data;
};

// Pipeline collections
struct RenderGraphPipelines {
    std::vector<ComputePipelineHandle> compute;
    std::vector<RasterPipelineHandle> raster;
    std::vector<RtPipelineHandle> rt;
};

// Compiled render graph
class CompiledRenderGraph {
public:
    RenderGraph rg;
    ResourceInfo resource_info;
    RenderGraphPipelines pipelines;

    ExecutingRenderGraph begin_execute(
        const RenderGraphExecutionParams& params,
        TransientResourceCache& transient_resource_cache,
        DynamicConstants& dynamic_constants
    );
};

// Executing render graph
class ExecutingRenderGraph {
public:
    void record_main_cb(CommandBuffer* cb);
    RetiredRenderGraph record_presentation_cb(CommandBuffer* cb, std::shared_ptr<Image> swapchain_image);

private:
    std::vector<RecordedPass> passes;
    std::vector<GraphResourceInfo> resources;
    std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>> exported_resources;
    ResourceRegistry resource_registry;

    static void record_pass_cb(RecordedPass pass, ResourceRegistry* resource_registry, CommandBuffer* cb);
    static void transition_resource(
        Device* device,
        CommandBuffer* cb,
        RegistryResource* resource,
        PassResourceAccessType access,
        bool debug,
        const std::string& dbg_str
    );
};

// Retired render graph
class RetiredRenderGraph {
public:
    template<typename Res>
    std::pair<const Res*, vk_sync::AccessType> exported_resource(ExportedHandle<Res> handle);

    void release_resources(TransientResourceCache& transient_resource_cache);

private:
    std::vector<RegistryResource> resources;
};

// Pass resource access types
enum class PassResourceAccessSyncType {
    AlwaysSync,
    SkipSyncIfSameAccessType
};

struct PassResourceAccessType {
    vk_sync::AccessType access_type;
    PassResourceAccessSyncType sync_type;

    static PassResourceAccessType new(vk_sync::AccessType access_type, PassResourceAccessSyncType sync_type) {
        return {access_type, sync_type};
    }
};

// Pass resource reference
struct PassResourceRef {
    GraphRawResourceHandle handle;
    PassResourceAccessType access;
};

// Recorded pass
struct RecordedPass {
    std::vector<PassResourceRef> read;
    std::vector<PassResourceRef> write;
    std::function<void(RenderPassApi*)> render_fn;
    std::string name;
    size_t idx;

    static RecordedPass new(const std::string& name, size_t idx) {
        return {
            .read = {},
            .write = {},
            .render_fn = {},
            .name = name,
            .idx = idx
        };
    }
};

} // namespace tekki::render_graph