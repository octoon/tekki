#pragma once

#include <functional>
#include <string>

#include "graph.h"
#include "pass_api.h"

namespace tekki::render_graph
{

class PassBuilder
{
public:
    PassBuilder(RenderGraph* rg, size_t pass_idx);
    ~PassBuilder();

    // Resource creation
    template <typename Desc> Handle<typename Desc::ResourceType> create(const Desc& desc);

    // Write operations
    template <typename Res, typename ViewType>
    Ref<Res, ViewType> write_impl(Handle<Res>* handle, backend::vulkan::AccessType access_type,
                                  PassResourceAccessSyncType sync_type);

    template <typename Res> Ref<Res, GpuUav> write(Handle<Res>* handle, backend::vulkan::AccessType access_type);

    template <typename Res> Ref<Res, GpuUav> write_no_sync(Handle<Res>* handle, backend::vulkan::AccessType access_type);

    template <typename Res> Ref<Res, GpuRt> raster(Handle<Res>* handle, backend::vulkan::AccessType access_type);

    // Read operations
    template <typename Res> Ref<Res, GpuSrv> read(const Handle<Res>* handle, backend::vulkan::AccessType access_type);

    template <typename Res> Ref<Res, GpuRt> raster_read(const Handle<Res>* handle, backend::vulkan::AccessType access_type);

    // Pipeline registration
    RgComputePipelineHandle register_compute_pipeline(const std::string& path);
    RgComputePipelineHandle register_compute_pipeline_with_desc(ComputePipelineDesc desc);

    RgRasterPipelineHandle register_raster_pipeline(const std::vector<PipelineShaderDesc>& shaders,
                                                    RasterPipelineDescBuilder desc);

    RgRtPipelineHandle register_ray_tracing_pipeline(const std::vector<PipelineShaderDesc>& shaders,
                                                     RayTracingPipelineDesc desc);

    // Render function
    void render(std::function<void(RenderPassApi*)> render_fn);

private:
    RenderGraph* rg;
    size_t pass_idx;
    std::optional<RecordedPass> pass;

    void validate_access_type(backend::vulkan::AccessType access_type, bool is_write);
};

// Access type validation helpers
namespace access_type_validation
{

inline bool is_valid_write_access(backend::vulkan::AccessType access_type)
{
    switch (access_type)
    {
    case backend::vulkan::AccessType::CommandBufferWriteNVX:
    case backend::vulkan::AccessType::VertexShaderWrite:
    case backend::vulkan::AccessType::TessellationControlShaderWrite:
    case backend::vulkan::AccessType::TessellationEvaluationShaderWrite:
    case backend::vulkan::AccessType::GeometryShaderWrite:
    case backend::vulkan::AccessType::FragmentShaderWrite:
    case backend::vulkan::AccessType::ComputeShaderWrite:
    case backend::vulkan::AccessType::AnyShaderWrite:
    case backend::vulkan::AccessType::TransferWrite:
    case backend::vulkan::AccessType::HostWrite:
    case backend::vulkan::AccessType::ColorAttachmentReadWrite:
    case backend::vulkan::AccessType::General:
        return true;
    default:
        return false;
    }
}

inline bool is_valid_raster_write_access(backend::vulkan::AccessType access_type)
{
    switch (access_type)
    {
    case backend::vulkan::AccessType::ColorAttachmentWrite:
    case backend::vulkan::AccessType::DepthStencilAttachmentWrite:
    case backend::vulkan::AccessType::DepthAttachmentWriteStencilReadOnly:
    case backend::vulkan::AccessType::StencilAttachmentWriteDepthReadOnly:
        return true;
    default:
        return false;
    }
}

inline bool is_valid_read_access(backend::vulkan::AccessType access_type)
{
    switch (access_type)
    {
    case backend::vulkan::AccessType::CommandBufferReadNVX:
    case backend::vulkan::AccessType::IndirectBuffer:
    case backend::vulkan::AccessType::IndexBuffer:
    case backend::vulkan::AccessType::VertexBuffer:
    case backend::vulkan::AccessType::VertexShaderReadUniformBuffer:
    case backend::vulkan::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::VertexShaderReadOther:
    case backend::vulkan::AccessType::TessellationControlShaderReadUniformBuffer:
    case backend::vulkan::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::TessellationControlShaderReadOther:
    case backend::vulkan::AccessType::TessellationEvaluationShaderReadUniformBuffer:
    case backend::vulkan::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::TessellationEvaluationShaderReadOther:
    case backend::vulkan::AccessType::GeometryShaderReadUniformBuffer:
    case backend::vulkan::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::GeometryShaderReadOther:
    case backend::vulkan::AccessType::FragmentShaderReadUniformBuffer:
    case backend::vulkan::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::FragmentShaderReadColorInputAttachment:
    case backend::vulkan::AccessType::FragmentShaderReadDepthStencilInputAttachment:
    case backend::vulkan::AccessType::FragmentShaderReadOther:
    case backend::vulkan::AccessType::ColorAttachmentRead:
    case backend::vulkan::AccessType::DepthStencilAttachmentRead:
    case backend::vulkan::AccessType::ComputeShaderReadUniformBuffer:
    case backend::vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::ComputeShaderReadOther:
    case backend::vulkan::AccessType::AnyShaderReadUniformBuffer:
    case backend::vulkan::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:
    case backend::vulkan::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:
    case backend::vulkan::AccessType::AnyShaderReadOther:
    case backend::vulkan::AccessType::TransferRead:
    case backend::vulkan::AccessType::HostRead:
    case backend::vulkan::AccessType::Present:
        return true;
    default:
        return false;
    }
}

inline bool is_valid_raster_read_access(backend::vulkan::AccessType access_type)
{
    switch (access_type)
    {
    case backend::vulkan::AccessType::ColorAttachmentRead:
    case backend::vulkan::AccessType::DepthStencilAttachmentRead:
        return true;
    default:
        return false;
    }
}

} // namespace access_type_validation

} // namespace tekki::render_graph