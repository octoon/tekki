#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../backend/vulkan/barrier.h"
#include "../backend/vulkan/device.h"
#include "../backend/vulkan/dynamic_constants.h"
#include "../backend/vulkan/shader.h"
#include "../backend/vulkan/transient_resource_cache.h"
#include "../backend/vulkan/buffer.h"
#include "../backend/vulkan/image.h"
#include "resource.h"
#include "resource_registry.h"

namespace tekki::render_graph
{

// Forward declarations
class PassBuilder;
struct RecordedPass;

// Forward declarations for backend types
namespace backend::vulkan {
    class CommandBuffer;
    class Device;
    class Image;
    class TransientResourceCache;
}

// Placeholder pipeline descriptions (to be properly defined later)
struct ComputePipelineDesc {};
struct RasterPipelineDesc {};
struct RayTracingPipelineDesc {};
struct PipelineShaderDesc {};

// Placeholder pipeline types (to be properly defined later)
class ComputePipeline {};
class RasterPipeline {};
class RayTracingPipeline {};

// Placeholder types (to be properly defined later)
struct FrameConstantsLayout {};
struct PendingDebugPass {};
struct CompiledRenderGraph {};

// Resource creation info
struct GraphResourceCreateInfo
{
    GraphResourceDesc desc;
};

// Resource import info
struct GraphResourceImportInfo
{
    enum class Type
    {
        Image,
        Buffer,
        RayTracingAcceleration,
        SwapchainImage
    };

    Type type;
    std::variant<std::shared_ptr<Image>, std::shared_ptr<Buffer>, std::shared_ptr<RayTracingAcceleration>> resource;
    backend::vulkan::AccessType access_type;
};

// Resource info
struct GraphResourceInfo
{
    enum class Type
    {
        Created,
        Imported
    };

    Type type;
    std::variant<GraphResourceCreateInfo, GraphResourceImportInfo> data;
};

// Exportable resource
struct ExportableGraphResource
{
    enum class Type
    {
        Image,
        Buffer
    };

    Type type;
    std::variant<Handle<ImageResource>, Handle<BufferResource>> data;

    GraphRawResourceHandle raw() const
    {
        switch (type)
        {
        case Type::Image:
            return std::get<Handle<ImageResource>>(data).raw;
        case Type::Buffer:
            return std::get<Handle<BufferResource>>(data).raw;
        }
    }
};

// Pipeline handles
struct RgComputePipelineHandle
{
    size_t id;
};

struct RgComputePipeline
{
    ComputePipelineDesc desc;
};

struct RgRasterPipelineHandle
{
    size_t id;
};

struct RgRasterPipeline
{
    std::vector<PipelineShaderDesc> shaders;
    RasterPipelineDesc desc;
};

struct RgRtPipelineHandle
{
    size_t id;
};

struct RgRtPipeline
{
    std::vector<PipelineShaderDesc> shaders;
    RayTracingPipelineDesc desc;
};

// Predefined descriptor set
struct PredefinedDescriptorSet
{
    std::unordered_map<uint32_t, rspirv_reflect::DescriptorInfo> bindings;
};

// Debug hooks
struct RenderDebugHook
{
    std::string name;
    uint64_t id;
};

struct GraphDebugHook
{
    RenderDebugHook render_debug_hook;
};

// Main render graph class
class RenderGraph
{
public:
    RenderGraph();
    ~RenderGraph() = default;

    // Resource creation
    template <typename Desc> Handle<typename ResourceDescTraits<Desc>::ResourceType> create(const Desc& desc);

    // Resource import/export
    template <typename Res> Handle<Res> import(std::shared_ptr<Res> resource, backend::vulkan::AccessType access_type);

    template <typename Res> ExportedHandle<Res> export(Handle<Res> resource, backend::vulkan::AccessType access_type);

    // Pass management
    PassBuilder add_pass(const std::string& name);

    // Compilation
    CompiledRenderGraph compile(PipelineCache* pipeline_cache);

    // Swapchain
    Handle<ImageResource> get_swap_chain();

private:
    std::vector<RecordedPass> passes;
    std::vector<GraphResourceInfo> resources;
    std::vector<std::pair<ExportableGraphResource, backend::vulkan::AccessType>> exported_resources;
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
template <typename Res> class ImportExportToRenderGraph
{
public:
    static Handle<Res> import(std::shared_ptr<Res> self, RenderGraph& rg, backend::vulkan::AccessType access_type);
    static ExportedHandle<Res> export(Handle<Res> resource, RenderGraph& rg, backend::vulkan::AccessType access_type);
};

// Type equality trait
template <typename T> struct TypeEquals
{
    using Other = T;
    static Other same(T value) { return value; }
};

// Execution parameters
struct RenderGraphExecutionParams
{
    backend::vulkan::Device* device;
    PipelineCache* pipeline_cache;
    vk::DescriptorSet frame_descriptor_set;
    FrameConstantsLayout frame_constants_layout;
    VkProfilerData* profiler_data;
};

// Pipeline collections
struct RenderGraphPipelines
{
    std::vector<RgComputePipelineHandle> compute;
    std::vector<RgRasterPipelineHandle> raster;
    std::vector<RgRtPipelineHandle> rt;
};

// Compiled render graph
class CompiledRenderGraph
{
public:
    RenderGraph rg;
    ResourceInfo resource_info;
    RenderGraphPipelines pipelines;

    ExecutingRenderGraph begin_execute(const RenderGraphExecutionParams& params,
                                       TransientResourceCache& transient_resource_cache,
                                       DynamicConstants& dynamic_constants);
};

// Executing render graph
class ExecutingRenderGraph
{
public:
    void record_main_cb(backend::vulkan::CommandBuffer* cb);
    RetiredRenderGraph record_presentation_cb(backend::vulkan::CommandBuffer* cb, std::shared_ptr<backend::vulkan::Image> swapchain_image);

private:
    std::vector<RecordedPass> passes;
    std::vector<GraphResourceInfo> resources;
    std::vector<std::pair<ExportableGraphResource, backend::vulkan::AccessType>> exported_resources;
    ResourceRegistry resource_registry;

    static void record_pass_cb(RecordedPass pass, ResourceRegistry* resource_registry, backend::vulkan::CommandBuffer* cb);
    static void transition_resource(backend::vulkan::Device* device, backend::vulkan::CommandBuffer* cb, RegistryResource* resource,
                                    PassResourceAccessType access, bool debug, const std::string& dbg_str);
};

// Retired render graph
class RetiredRenderGraph
{
public:
    template <typename Res> std::pair<const Res*, backend::vulkan::AccessType> exported_resource(ExportedHandle<Res> handle);

    void release_resources(TransientResourceCache& transient_resource_cache);

private:
    std::vector<RegistryResource> resources;
};

// Pass resource access types
enum class PassResourceAccessSyncType
{
    AlwaysSync,
    SkipSyncIfSameAccessType
};

struct PassResourceAccessType
{
    ::tekki::backend::vulkan::AccessType access_type;
    PassResourceAccessSyncType sync_type;

    static PassResourceAccessType create(::tekki::backend::vulkan::AccessType access_type, PassResourceAccessSyncType sync_type)
    {
        PassResourceAccessType result;
        result.access_type = access_type;
        result.sync_type = sync_type;
        return result;
    }
};

// Pass resource reference
struct PassResourceRef
{
    GraphRawResourceHandle handle;
    PassResourceAccessType access;
};

// Recorded pass
struct RecordedPass
{
    std::vector<PassResourceRef> read;
    std::vector<PassResourceRef> write;
    std::function<void(RenderPassApi*)> render_fn;
    std::string name;
    size_t idx;

    static RecordedPass create(const std::string& name, size_t idx)
    {
        RecordedPass result;
        result.read = {};
        result.write = {};
        result.render_fn = {};
        result.name = name;
        result.idx = idx;
        return result;
    }
};

} // namespace tekki::render_graph