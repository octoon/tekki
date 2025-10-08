#pragma once

#include "Graph.h"
#include "PassApi.h"

#include <string>
#include <functional>

namespace tekki::render_graph {

class PassBuilder {
public:
    PassBuilder(RenderGraph* rg, size_t pass_idx);
    ~PassBuilder();

    // Resource creation
    template<typename Desc>
    Handle<typename Desc::ResourceType> create(const Desc& desc);

    // Write operations
    template<typename Res, typename ViewType>
    Ref<Res, ViewType> write_impl(Handle<Res>* handle, vk_sync::AccessType access_type, PassResourceAccessSyncType sync_type);

    template<typename Res>
    Ref<Res, GpuUav> write(Handle<Res>* handle, vk_sync::AccessType access_type);

    template<typename Res>
    Ref<Res, GpuUav> write_no_sync(Handle<Res>* handle, vk_sync::AccessType access_type);

    template<typename Res>
    Ref<Res, GpuRt> raster(Handle<Res>* handle, vk_sync::AccessType access_type);

    // Read operations
    template<typename Res>
    Ref<Res, GpuSrv> read(const Handle<Res>* handle, vk_sync::AccessType access_type);

    template<typename Res>
    Ref<Res, GpuRt> raster_read(const Handle<Res>* handle, vk_sync::AccessType access_type);

    // Pipeline registration
    RgComputePipelineHandle register_compute_pipeline(const std::string& path);
    RgComputePipelineHandle register_compute_pipeline_with_desc(ComputePipelineDesc desc);

    RgRasterPipelineHandle register_raster_pipeline(
        const std::vector<PipelineShaderDesc>& shaders,
        RasterPipelineDescBuilder desc
    );

    RgRtPipelineHandle register_ray_tracing_pipeline(
        const std::vector<PipelineShaderDesc>& shaders,
        RayTracingPipelineDesc desc
    );

    // Render function
    void render(std::function<void(RenderPassApi*)> render_fn);

private:
    RenderGraph* rg;
    size_t pass_idx;
    std::optional<RecordedPass> pass;

    void validate_access_type(vk_sync::AccessType access_type, bool is_write);
};

// Access type validation helpers
namespace access_type_validation {

inline bool is_valid_write_access(vk_sync::AccessType access_type) {
    switch (access_type) {
        case vk_sync::AccessType::CommandBufferWriteNVX:
        case vk_sync::AccessType::VertexShaderWrite:
        case vk_sync::AccessType::TessellationControlShaderWrite:
        case vk_sync::AccessType::TessellationEvaluationShaderWrite:
        case vk_sync::AccessType::GeometryShaderWrite:
        case vk_sync::AccessType::FragmentShaderWrite:
        case vk_sync::AccessType::ComputeShaderWrite:
        case vk_sync::AccessType::AnyShaderWrite:
        case vk_sync::AccessType::TransferWrite:
        case vk_sync::AccessType::HostWrite:
        case vk_sync::AccessType::ColorAttachmentReadWrite:
        case vk_sync::AccessType::General:
            return true;
        default:
            return false;
    }
}

inline bool is_valid_raster_write_access(vk_sync::AccessType access_type) {
    switch (access_type) {
        case vk_sync::AccessType::ColorAttachmentWrite:
        case vk_sync::AccessType::DepthStencilAttachmentWrite:
        case vk_sync::AccessType::DepthAttachmentWriteStencilReadOnly:
        case vk_sync::AccessType::StencilAttachmentWriteDepthReadOnly:
            return true;
        default:
            return false;
    }
}

inline bool is_valid_read_access(vk_sync::AccessType access_type) {
    switch (access_type) {
        case vk_sync::AccessType::CommandBufferReadNVX:
        case vk_sync::AccessType::IndirectBuffer:
        case vk_sync::AccessType::IndexBuffer:
        case vk_sync::AccessType::VertexBuffer:
        case vk_sync::AccessType::VertexShaderReadUniformBuffer:
        case vk_sync::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::VertexShaderReadOther:
        case vk_sync::AccessType::TessellationControlShaderReadUniformBuffer:
        case vk_sync::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::TessellationControlShaderReadOther:
        case vk_sync::AccessType::TessellationEvaluationShaderReadUniformBuffer:
        case vk_sync::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::TessellationEvaluationShaderReadOther:
        case vk_sync::AccessType::GeometryShaderReadUniformBuffer:
        case vk_sync::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::GeometryShaderReadOther:
        case vk_sync::AccessType::FragmentShaderReadUniformBuffer:
        case vk_sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::FragmentShaderReadColorInputAttachment:
        case vk_sync::AccessType::FragmentShaderReadDepthStencilInputAttachment:
        case vk_sync::AccessType::FragmentShaderReadOther:
        case vk_sync::AccessType::ColorAttachmentRead:
        case vk_sync::AccessType::DepthStencilAttachmentRead:
        case vk_sync::AccessType::ComputeShaderReadUniformBuffer:
        case vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::ComputeShaderReadOther:
        case vk_sync::AccessType::AnyShaderReadUniformBuffer:
        case vk_sync::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:
        case vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:
        case vk_sync::AccessType::AnyShaderReadOther:
        case vk_sync::AccessType::TransferRead:
        case vk_sync::AccessType::HostRead:
        case vk_sync::AccessType::Present:
            return true;
        default:
            return false;
    }
}

inline bool is_valid_raster_read_access(vk_sync::AccessType access_type) {
    switch (access_type) {
        case vk_sync::AccessType::ColorAttachmentRead:
        case vk_sync::AccessType::DepthStencilAttachmentRead:
            return true;
        default:
            return false;
    }
}

} // namespace access_type_validation

} // namespace tekki::render_graph