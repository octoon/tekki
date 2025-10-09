#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include "resource.h"

namespace tekki::render_graph {

// Pipeline collections
struct RenderGraphPipelines
{
    std::vector<RgComputePipelineHandle> compute;
    std::vector<RgRasterPipelineHandle> raster;
    std::vector<RgRtPipelineHandle> rt;
};

// Forward declarations
struct FrameConstantsLayout {};

// Execution parameters
struct RenderGraphExecutionParams
{
    backend::vulkan::Device* device;
    backend::vulkan::PipelineCache* pipeline_cache;
    vk::DescriptorSet frame_descriptor_set;
    FrameConstantsLayout frame_constants_layout;
    backend::vulkan::GpuProfiler* profiler_data;
};

} // namespace tekki::render_graph