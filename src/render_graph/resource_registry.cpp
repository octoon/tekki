#include "tekki/render_graph/resource_registry.h"

namespace tekki {
namespace render_graph {

// AnyRenderResource methods
AnyRenderResourceRef AnyRenderResource::borrow() const {
    switch (type) {
        case Type::OwnedImage:
            return AnyRenderResourceRef{
                .type = AnyRenderResourceRef::Type::Image,
                .data = &std::get<Image>(data)
            };
        case Type::ImportedImage:
            return AnyRenderResourceRef{
                .type = AnyRenderResourceRef::Type::Image,
                .data = std::get<std::shared_ptr<Image>>(data).get()
            };
        case Type::OwnedBuffer:
            return AnyRenderResourceRef{
                .type = AnyRenderResourceRef::Type::Buffer,
                .data = &std::get<Buffer>(data)
            };
        case Type::ImportedBuffer:
            return AnyRenderResourceRef{
                .type = AnyRenderResourceRef::Type::Buffer,
                .data = std::get<std::shared_ptr<Buffer>>(data).get()
            };
        case Type::ImportedRayTracingAcceleration:
            return AnyRenderResourceRef{
                .type = AnyRenderResourceRef::Type::RayTracingAcceleration,
                .data = std::get<std::shared_ptr<RayTracingAcceleration>>(data).get()
            };
        case Type::Pending:
            throw std::runtime_error("Cannot borrow pending resource");
    }
    throw std::runtime_error("Unknown resource type");
}

// ResourceRegistry pipeline access methods
std::shared_ptr<ComputePipeline> ResourceRegistry::compute_pipeline(RgComputePipelineHandle pipeline) {
    auto& pipelineInfo = pipelines.compute[pipeline.id];
    return pipelineInfo.pipeline;
}

std::shared_ptr<RasterPipeline> ResourceRegistry::raster_pipeline(RgRasterPipelineHandle pipeline) {
    auto& pipelineInfo = pipelines.raster[pipeline.id];
    return pipelineInfo.pipeline;
}

std::shared_ptr<RayTracingPipeline> ResourceRegistry::ray_tracing_pipeline(RgRtPipelineHandle pipeline) {
    auto& pipelineInfo = pipelines.rt[pipeline.id];
    return pipelineInfo.pipeline;
}

} // namespace render_graph
} // namespace tekki