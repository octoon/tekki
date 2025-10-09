#pragma once

#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/shader_compiler.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>

namespace tekki::backend::vulkan {

// Handle types for different pipeline types
struct ComputePipelineHandle {
    size_t index;

    bool operator==(const ComputePipelineHandle& other) const { return index == other.index; }
    bool operator!=(const ComputePipelineHandle& other) const { return index != other.index; }
};

struct RasterPipelineHandle {
    size_t index;

    bool operator==(const RasterPipelineHandle& other) const { return index == other.index; }
    bool operator!=(const RasterPipelineHandle& other) const { return index != other.index; }
};

struct RtPipelineHandle {
    size_t index;

    bool operator==(const RtPipelineHandle& other) const { return index == other.index; }
    bool operator!=(const RtPipelineHandle& other) const { return index != other.index; }
};

// Hash functions for handles
struct ComputePipelineHandleHash {
    size_t operator()(const ComputePipelineHandle& handle) const { return std::hash<size_t>{}(handle.index); }
};

struct RasterPipelineHandleHash {
    size_t operator()(const RasterPipelineHandle& handle) const { return std::hash<size_t>{}(handle.index); }
};

struct RtPipelineHandleHash {
    size_t operator()(const RtPipelineHandle& handle) const { return std::hash<size_t>{}(handle.index); }
};

// Forward declarations
class ShaderCompiler;

// Pipeline cache entry for compute pipelines
struct ComputePipelineCacheEntry {
    ComputePipelineDesc desc;
    std::shared_ptr<ComputePipeline> pipeline;
    bool is_compiling = false;
    bool needs_recompile = false;
};

// Pipeline cache entry for raster pipelines
struct RasterPipelineCacheEntry {
    RasterPipelineDesc desc;
    std::vector<PipelineShaderDesc> shader_descs;
    std::shared_ptr<RasterPipeline> pipeline;
    bool is_compiling = false;
    bool needs_recompile = false;
};

// Pipeline cache entry for ray tracing pipelines
struct RtPipelineCacheEntry {
    RayTracingPipelineDesc desc;
    std::vector<PipelineShaderDesc> shader_descs;
    std::shared_ptr<RayTracingPipeline> pipeline;
    bool is_compiling = false;
    bool needs_recompile = false;
};

// Main pipeline cache
class PipelineCache {
public:
    PipelineCache(Device* device, ShaderCompiler* shader_compiler);
    ~PipelineCache();

    // Register pipelines and get handles
    ComputePipelineHandle RegisterCompute(const ComputePipelineDesc& desc);
    RasterPipelineHandle RegisterRaster(const std::vector<PipelineShaderDesc>& shaders,
                                        const RasterPipelineDesc& desc);
    RtPipelineHandle RegisterRayTracing(const std::vector<PipelineShaderDesc>& shaders,
                                       const RayTracingPipelineDesc& desc);

    // Get compiled pipelines
    std::shared_ptr<ComputePipeline> GetCompute(ComputePipelineHandle handle);
    std::shared_ptr<RasterPipeline> GetRaster(RasterPipelineHandle handle);
    std::shared_ptr<RayTracingPipeline> GetRayTracing(RtPipelineHandle handle);

    // Compile all pending shaders in parallel
    void ParallelCompileShaders();

    // Prepare for the next frame (invalidate stale pipelines and compile new ones)
    void PrepareFrame();

private:
    void InvalidateStalePipelines();

    Device* device_;
    ShaderCompiler* shader_compiler_;

    std::unordered_map<ComputePipelineHandle, ComputePipelineCacheEntry, ComputePipelineHandleHash> compute_entries_;
    std::unordered_map<RasterPipelineHandle, RasterPipelineCacheEntry, RasterPipelineHandleHash> raster_entries_;
    std::unordered_map<RtPipelineHandle, RtPipelineCacheEntry, RtPipelineHandleHash> rt_entries_;

    // Reverse mappings for deduplication
    std::unordered_map<std::string, ComputePipelineHandle> compute_shader_to_handle_;
    std::unordered_map<std::string, RasterPipelineHandle> raster_shaders_to_handle_;
    std::unordered_map<std::string, RtPipelineHandle> rt_shaders_to_handle_;

    size_t next_compute_handle_ = 0;
    size_t next_raster_handle_ = 0;
    size_t next_rt_handle_ = 0;

    mutable std::mutex mutex_;
};

} // namespace tekki::backend::vulkan
