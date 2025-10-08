#include "tekki/render_graph/pass_builder.h"

namespace tekki {
namespace render_graph {

// PassBuilder constructor and destructor
PassBuilder::PassBuilder(RenderGraph* rg, size_t pass_idx)
    : rg(rg), pass_idx(pass_idx), pass(RecordedPass::new_("", pass_idx)) {}

PassBuilder::~PassBuilder() {
    if (pass) {
        rg->record_pass(std::move(*pass));
    }
}

// Resource creation
template<typename Desc>
Handle<typename Desc::ResourceType> PassBuilder::create(const Desc& desc) {
    return rg->create(desc);
}

// Write operations
template<typename Res, typename ViewType>
Ref<Res, ViewType> PassBuilder::write_impl(
    Handle<Res>* handle,
    backend::vulkan::AccessType access_type,
    PassResourceAccessSyncType sync_type) {

    validate_access_type(access_type, true);

    auto pass_access = PassResourceAccessType::new_(access_type, sync_type);
    auto ref = Ref<Res, ViewType>{
        .handle = handle->raw,
        .desc = handle->desc
    };

    pass->write.push_back(PassResourceRef{
        .handle = handle->raw,
        .access = pass_access
    });

    return ref;
}

template<typename Res>
Ref<Res, GpuUav> PassBuilder::write(Handle<Res>* handle, backend::vulkan::AccessType access_type) {
    return write_impl<Res, GpuUav>(handle, access_type, PassResourceAccessSyncType::AlwaysSync);
}

template<typename Res>
Ref<Res, GpuUav> PassBuilder::write_no_sync(Handle<Res>* handle, backend::vulkan::AccessType access_type) {
    return write_impl<Res, GpuUav>(handle, access_type, PassResourceAccessSyncType::SkipSyncIfSameAccessType);
}

template<typename Res>
Ref<Res, GpuRt> PassBuilder::raster(Handle<Res>* handle, backend::vulkan::AccessType access_type) {
    return write_impl<Res, GpuRt>(handle, access_type, PassResourceAccessSyncType::AlwaysSync);
}

// Read operations
template<typename Res>
Ref<Res, GpuSrv> PassBuilder::read(const Handle<Res>* handle, backend::vulkan::AccessType access_type) {
    validate_access_type(access_type, false);

    auto pass_access = PassResourceAccessType::new_(access_type, PassResourceAccessSyncType::AlwaysSync);
    auto ref = Ref<Res, GpuSrv>{
        .handle = handle->raw,
        .desc = handle->desc
    };

    pass->read.push_back(PassResourceRef{
        .handle = handle->raw,
        .access = pass_access
    });

    return ref;
}

template<typename Res>
Ref<Res, GpuRt> PassBuilder::raster_read(const Handle<Res>* handle, backend::vulkan::AccessType access_type) {
    validate_access_type(access_type, false);

    auto pass_access = PassResourceAccessType::new_(access_type, PassResourceAccessSyncType::AlwaysSync);
    auto ref = Ref<Res, GpuRt>{
        .handle = handle->raw,
        .desc = handle->desc
    };

    pass->read.push_back(PassResourceRef{
        .handle = handle->raw,
        .access = pass_access
    });

    return ref;
}

// Pipeline registration
RgComputePipelineHandle PassBuilder::register_compute_pipeline(const std::string& path) {
    auto pipeline = RgComputePipeline{
        .desc = ComputePipelineDesc::builder()
            .compute(path)
            .build()
    };

    auto handle = RgComputePipelineHandle{
        .id = rg->compute_pipelines.size()
    };

    rg->compute_pipelines.push_back(std::move(pipeline));
    return handle;
}

RgComputePipelineHandle PassBuilder::register_compute_pipeline_with_desc(ComputePipelineDesc desc) {
    auto pipeline = RgComputePipeline{
        .desc = std::move(desc)
    };

    auto handle = RgComputePipelineHandle{
        .id = rg->compute_pipelines.size()
    };

    rg->compute_pipelines.push_back(std::move(pipeline));
    return handle;
}

RgRasterPipelineHandle PassBuilder::register_raster_pipeline(
    const std::vector<PipelineShaderDesc>& shaders,
    RasterPipelineDescBuilder desc) {

    auto pipeline = RgRasterPipeline{
        .shaders = shaders,
        .desc = desc.build()
    };

    auto handle = RgRasterPipelineHandle{
        .id = rg->raster_pipelines.size()
    };

    rg->raster_pipelines.push_back(std::move(pipeline));
    return handle;
}

RgRtPipelineHandle PassBuilder::register_ray_tracing_pipeline(
    const std::vector<PipelineShaderDesc>& shaders,
    RayTracingPipelineDesc desc) {

    auto pipeline = RgRtPipeline{
        .shaders = shaders,
        .desc = std::move(desc)
    };

    auto handle = RgRtPipelineHandle{
        .id = rg->rt_pipelines.size()
    };

    rg->rt_pipelines.push_back(std::move(pipeline));
    return handle;
}

// Render function
void PassBuilder::render(std::function<void(RenderPassApi*)> render_fn) {
    pass->render_fn = std::move(render_fn);
}

// Access type validation
void PassBuilder::validate_access_type(backend::vulkan::AccessType access_type, bool is_write) {
    if (is_write) {
        if (!access_type_validation::is_valid_write_access(access_type)) {
            throw std::runtime_error("Invalid write access type");
        }
    } else {
        if (!access_type_validation::is_valid_read_access(access_type)) {
            throw std::runtime_error("Invalid read access type");
        }
    }
}

// Explicit template instantiations
template Handle<ImageResource> PassBuilder::create<ImageDesc>(const ImageDesc&);
template Handle<BufferResource> PassBuilder::create<BufferDesc>(const BufferDesc&);
template Handle<RayTracingAccelerationResource> PassBuilder::create<RayTracingAccelerationDesc>(const RayTracingAccelerationDesc&);

template Ref<ImageResource, GpuUav> PassBuilder::write<ImageResource>(Handle<ImageResource>*, backend::vulkan::AccessType);
template Ref<BufferResource, GpuUav> PassBuilder::write<BufferResource>(Handle<BufferResource>*, backend::vulkan::AccessType);
template Ref<RayTracingAccelerationResource, GpuUav> PassBuilder::write<RayTracingAccelerationResource>(Handle<RayTracingAccelerationResource>*, backend::vulkan::AccessType);

template Ref<ImageResource, GpuUav> PassBuilder::write_no_sync<ImageResource>(Handle<ImageResource>*, backend::vulkan::AccessType);
template Ref<BufferResource, GpuUav> PassBuilder::write_no_sync<BufferResource>(Handle<BufferResource>*, backend::vulkan::AccessType);
template Ref<RayTracingAccelerationResource, GpuUav> PassBuilder::write_no_sync<RayTracingAccelerationResource>(Handle<RayTracingAccelerationResource>*, backend::vulkan::AccessType);

template Ref<ImageResource, GpuRt> PassBuilder::raster<ImageResource>(Handle<ImageResource>*, backend::vulkan::AccessType);
template Ref<BufferResource, GpuRt> PassBuilder::raster<BufferResource>(Handle<BufferResource>*, backend::vulkan::AccessType);
template Ref<RayTracingAccelerationResource, GpuRt> PassBuilder::raster<RayTracingAccelerationResource>(Handle<RayTracingAccelerationResource>*, backend::vulkan::AccessType);

template Ref<ImageResource, GpuSrv> PassBuilder::read<ImageResource>(const Handle<ImageResource>*, backend::vulkan::AccessType);
template Ref<BufferResource, GpuSrv> PassBuilder::read<BufferResource>(const Handle<BufferResource>*, backend::vulkan::AccessType);
template Ref<RayTracingAccelerationResource, GpuSrv> PassBuilder::read<RayTracingAccelerationResource>(const Handle<RayTracingAccelerationResource>*, backend::vulkan::AccessType);

template Ref<ImageResource, GpuRt> PassBuilder::raster_read<ImageResource>(const Handle<ImageResource>*, backend::vulkan::AccessType);
template Ref<BufferResource, GpuRt> PassBuilder::raster_read<BufferResource>(const Handle<BufferResource>*, backend::vulkan::AccessType);
template Ref<RayTracingAccelerationResource, GpuRt> PassBuilder::raster_read<RayTracingAccelerationResource>(const Handle<RayTracingAccelerationResource>*, backend::vulkan::AccessType);

} // namespace render_graph
} // namespace tekki