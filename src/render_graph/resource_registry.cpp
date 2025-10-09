#include "tekki/render_graph/resource_registry.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/pipeline_cache.h"

namespace tekki::render_graph {

std::shared_ptr<ComputePipeline> ResourceRegistry::compute_pipeline(RgComputePipelineHandle pipeline)
{
    auto handle = pipelines.compute[pipeline.id];
    return execution_params->pipeline_cache->GetCompute(handle);
}

std::shared_ptr<RasterPipeline> ResourceRegistry::raster_pipeline(RgRasterPipelineHandle pipeline)
{
    auto handle = pipelines.raster[pipeline.id];
    return execution_params->pipeline_cache->GetRaster(handle);
}

std::shared_ptr<RayTracingPipeline> ResourceRegistry::ray_tracing_pipeline(RgRtPipelineHandle pipeline)
{
    auto handle = pipelines.rt[pipeline.id];
    return execution_params->pipeline_cache->GetRayTracing(handle);
}

} // namespace tekki::render_graph