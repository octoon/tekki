#include "tekki/render_graph/graph.h"

namespace tekki {
namespace render_graph {

// RenderGraph constructor
RenderGraph::RenderGraph()
    : passes(), resources(), exported_resources(), compute_pipelines(), raster_pipelines(), rt_pipelines(),
      predefined_descriptor_set_layouts(), debug_hook(std::nullopt), debugged_resource(std::nullopt) {}

// Resource creation
template<typename Desc>
Handle<typename ResourceDescTraits<Desc>::ResourceType> RenderGraph::create(const Desc& desc) {
    auto create_info = GraphResourceCreateInfo{
        .desc = desc
    };

    auto raw_handle = create_raw_resource(create_info);
    auto resource_info = GraphResourceInfo{
        .type = GraphResourceInfo::Type::Created,
        .data = create_info
    };

    resources.push_back(resource_info);

    return Handle<typename ResourceDescTraits<Desc>::ResourceType>{
        .raw = raw_handle,
        .desc = desc
    };
}

// Resource import
template<typename Res>
Handle<Res> RenderGraph::import(std::shared_ptr<Res> resource, backend::vulkan::AccessType access_type) {
    auto import_info = GraphResourceImportInfo{
        .type = std::is_same_v<Res, ImageResource> ? GraphResourceImportInfo::Type::Image :
                std::is_same_v<Res, BufferResource> ? GraphResourceImportInfo::Type::Buffer :
                GraphResourceImportInfo::Type::RayTracingAcceleration,
        .resource = resource,
        .access_type = access_type
    };

    auto raw_handle = create_raw_resource(GraphResourceCreateInfo{
        .desc = typename ResourceTraits<Res>::DescType{}
    });

    auto resource_info = GraphResourceInfo{
        .type = GraphResourceInfo::Type::Imported,
        .data = import_info
    };

    resources.push_back(resource_info);

    return Handle<Res>{
        .raw = raw_handle,
        .desc = typename ResourceTraits<Res>::DescType{}
    };
}

// Resource export
template<typename Res>
ExportedHandle<Res> RenderGraph::export(Handle<Res> resource, backend::vulkan::AccessType access_type) {
    ExportableGraphResource exportable{
        .type = std::is_same_v<Res, ImageResource> ? ExportableGraphResource::Type::Image :
                std::is_same_v<Res, BufferResource> ? ExportableGraphResource::Type::Buffer :
                throw std::runtime_error("Unsupported resource type for export"),
        .data = resource
    };

    exported_resources.emplace_back(std::move(exportable), access_type);

    return ExportedHandle<Res>{
        .raw = resource.raw
    };
}

// Pass management
PassBuilder RenderGraph::add_pass(const std::string& name) {
    auto pass_idx = passes.size();
    auto pass = RecordedPass::create(name, pass_idx);
    passes.push_back(std::move(pass));
    return PassBuilder(this, pass_idx);
}

// Compilation
CompiledRenderGraph RenderGraph::compile(PipelineCache* pipeline_cache) {
    auto resource_info = calculate_resource_info();

    // Compile pipelines
    RenderGraphPipelines pipelines;

    for (const auto& compute_pipeline : compute_pipelines) {
        auto handle = pipeline_cache->get_or_create_compute_pipeline(compute_pipeline.desc);
        pipelines.compute.push_back(handle);
    }

    for (const auto& raster_pipeline : raster_pipelines) {
        auto handle = pipeline_cache->get_or_create_raster_pipeline(raster_pipeline.shaders, raster_pipeline.desc);
        pipelines.raster.push_back(handle);
    }

    for (const auto& rt_pipeline : rt_pipelines) {
        auto handle = pipeline_cache->get_or_create_ray_tracing_pipeline(rt_pipeline.shaders, rt_pipeline.desc);
        pipelines.rt.push_back(handle);
    }

    return CompiledRenderGraph{
        .rg = *this,
        .resource_info = std::move(resource_info),
        .pipelines = std::move(pipelines)
    };
}

// Swapchain
Handle<ImageResource> RenderGraph::get_swap_chain() {
    // Implementation for getting swapchain image
    // This would typically return a handle to the swapchain image
    throw std::runtime_error("get_swap_chain not implemented");
}

// Private methods
GraphRawResourceHandle RenderGraph::create_raw_resource(const GraphResourceCreateInfo& info) {
    auto id = static_cast<uint32_t>(resources.size());
    return GraphRawResourceHandle{
        .id = id,
        .version = 0
    };
}

void RenderGraph::record_pass(RecordedPass pass) {
    passes[pass.idx] = std::move(pass);
}

ResourceInfo RenderGraph::calculate_resource_info() const {
    ResourceInfo info;

    // Calculate resource dependencies and scheduling
    // This is a simplified implementation

    for (const auto& resource : resources) {
        // Process each resource
    }

    for (const auto& pass : passes) {
        // Process each pass
    }

    return info;
}

std::optional<PendingDebugPass> RenderGraph::hook_debug_pass(const RecordedPass& pass) {
    if (debug_hook && debugged_resource) {
        // Create debug pass if needed
        return std::nullopt;
    }
    return std::nullopt;
}

// Import/export trait implementation
template<typename Res>
Handle<Res> ImportExportToRenderGraph<Res>::import(std::shared_ptr<Res> self, RenderGraph& rg, backend::vulkan::AccessType access_type) {
    return rg.import(std::move(self), access_type);
}

template<typename Res>
ExportedHandle<Res> ImportExportToRenderGraph<Res>::export(Handle<Res> resource, RenderGraph& rg, backend::vulkan::AccessType access_type) {
    return rg.export(std::move(resource), access_type);
}

// CompiledRenderGraph methods
ExecutingRenderGraph CompiledRenderGraph::begin_execute(
    const RenderGraphExecutionParams& params,
    TransientResourceCache& transient_resource_cache,
    DynamicConstants& dynamic_constants) {

    ExecutingRenderGraph executing;
    executing.passes = rg.passes;
    executing.resources = rg.resources;
    executing.exported_resources = rg.exported_resources;
    executing.resource_registry.execution_params = const_cast<RenderGraphExecutionParams*>(&params);
    executing.resource_registry.dynamic_constants = &dynamic_constants;
    executing.resource_registry.pipelines = pipelines;

    // Initialize resource registry with resources
    executing.resource_registry.resources.resize(rg.resources.size());

    for (size_t i = 0; i < rg.resources.size(); ++i) {
        const auto& resource_info = rg.resources[i];

        if (resource_info.type == GraphResourceInfo::Type::Created) {
            const auto& create_info = std::get<GraphResourceCreateInfo>(resource_info.data);

            std::visit(overloaded{
                [&](const ImageDesc& desc) {
                    auto image = transient_resource_cache.get_or_create_image(desc);
                    executing.resource_registry.resources[i] = RegistryResource{
                        .resource = AnyRenderResource{
                            .type = AnyRenderResource::Type::OwnedImage,
                            .data = std::move(image)
                        },
                        .access_type = backend::vulkan::AccessType::Nothing
                    };
                },
                [&](const BufferDesc& desc) {
                    auto buffer = transient_resource_cache.get_or_create_buffer(desc);
                    executing.resource_registry.resources[i] = RegistryResource{
                        .resource = AnyRenderResource{
                            .type = AnyRenderResource::Type::OwnedBuffer,
                            .data = std::move(buffer)
                        },
                        .access_type = backend::vulkan::AccessType::Nothing
                    };
                },
                [&](const auto&) {
                    throw std::runtime_error("Unsupported resource type");
                }
            }, create_info.desc);
        }
    }

    return executing;
}

// ExecutingRenderGraph methods
void ExecutingRenderGraph::record_main_cb(CommandBuffer* cb) {
    for (const auto& pass : passes) {
        record_pass_cb(pass, &resource_registry, cb);
    }
}

RetiredRenderGraph ExecutingRenderGraph::record_presentation_cb(CommandBuffer* cb, std::shared_ptr<Image> swapchain_image) {
    // Record presentation-specific passes
    // This typically includes final composition and presentation preparation

    return RetiredRenderGraph{
        .resources = std::move(resource_registry.resources)
    };
}

void ExecutingRenderGraph::record_pass_cb(RecordedPass pass, ResourceRegistry* resource_registry, CommandBuffer* cb) {
    // Set up render pass API
    RenderPassApi api{
        .cb = cb,
        .resources = resource_registry
    };

    // Execute the pass
    if (pass.render_fn) {
        pass.render_fn(&api);
    }
}

void ExecutingRenderGraph::transition_resource(
    Device* device,
    CommandBuffer* cb,
    RegistryResource* resource,
    PassResourceAccessType access,
    bool debug,
    const std::string& dbg_str) {

    // Implementation for resource transition
    // This would typically handle image and buffer layout transitions
}

// RetiredRenderGraph methods
template<typename Res>
std::pair<const Res*, backend::vulkan::AccessType> RetiredRenderGraph::exported_resource(ExportedHandle<Res> handle) {
    // Find the exported resource
    for (const auto& resource : resources) {
        if (resource.resource.borrow().type == AnyRenderResourceRef::Type::Image &&
            std::get<const Image*>(resource.resource.borrow().data)->raw() == handle.raw.id) {

            return {
                std::get<const Image*>(resource.resource.borrow().data),
                resource.access_type
            };
        }
    }

    throw std::runtime_error("Exported resource not found");
}

void RetiredRenderGraph::release_resources(TransientResourceCache& transient_resource_cache) {
    // Release transient resources back to the cache
    for (auto& resource : resources) {
        if (resource.resource.type == AnyRenderResource::Type::OwnedImage) {
            transient_resource_cache.release_image(std::get<Image>(resource.resource.data));
        } else if (resource.resource.type == AnyRenderResource::Type::OwnedBuffer) {
            transient_resource_cache.release_buffer(std::get<Buffer>(resource.resource.data));
        }
    }
}

// Explicit template instantiations
template Handle<ImageResource> RenderGraph::create<ImageDesc>(const ImageDesc&);
template Handle<BufferResource> RenderGraph::create<BufferDesc>(const BufferDesc&);
template Handle<RayTracingAccelerationResource> RenderGraph::create<RayTracingAccelerationDesc>(const RayTracingAccelerationDesc&);

template Handle<ImageResource> RenderGraph::import<ImageResource>(std::shared_ptr<ImageResource>, backend::vulkan::AccessType);
template Handle<BufferResource> RenderGraph::import<BufferResource>(std::shared_ptr<BufferResource>, backend::vulkan::AccessType);
template Handle<RayTracingAccelerationResource> RenderGraph::import<RayTracingAccelerationResource>(std::shared_ptr<RayTracingAccelerationResource>, backend::vulkan::AccessType);

template ExportedHandle<ImageResource> RenderGraph::export<ImageResource>(Handle<ImageResource>, backend::vulkan::AccessType);
template ExportedHandle<BufferResource> RenderGraph::export<BufferResource>(Handle<BufferResource>, backend::vulkan::AccessType);

template Handle<ImageResource> ImportExportToRenderGraph<ImageResource>::import(std::shared_ptr<ImageResource>, RenderGraph&, backend::vulkan::AccessType);
template Handle<BufferResource> ImportExportToRenderGraph<BufferResource>::import(std::shared_ptr<BufferResource>, RenderGraph&, backend::vulkan::AccessType);
template Handle<RayTracingAccelerationResource> ImportExportToRenderGraph<RayTracingAccelerationResource>::import(std::shared_ptr<RayTracingAccelerationResource>, RenderGraph&, backend::vulkan::AccessType);

template ExportedHandle<ImageResource> ImportExportToRenderGraph<ImageResource>::export(Handle<ImageResource>, RenderGraph&, backend::vulkan::AccessType);
template ExportedHandle<BufferResource> ImportExportToRenderGraph<BufferResource>::export(Handle<BufferResource>, RenderGraph&, backend::vulkan::AccessType);

template std::pair<const ImageResource*, backend::vulkan::AccessType> RetiredRenderGraph::exported_resource<ImageResource>(ExportedHandle<ImageResource>);
template std::pair<const BufferResource*, backend::vulkan::AccessType> RetiredRenderGraph::exported_resource<BufferResource>(ExportedHandle<BufferResource>);

} // namespace render_graph
} // namespace tekki