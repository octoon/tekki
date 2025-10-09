#include "tekki/backend/vulkan/pipeline_cache.h"
#include "tekki/backend/shader_compiler.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

namespace tekki::backend::vulkan {

namespace {

// Helper to create a unique key for shader source
std::string GetShaderSourceKey(const ShaderSource& source) {
    std::ostringstream oss;
    if (source.type == ShaderSourceType::Rust) {
        oss << "rust:" << source.entry;
    } else {
        oss << "hlsl:" << source.path.string();
    }
    return oss.str();
}

// Helper to create a unique key for multiple shader sources
std::string GetShadersKey(const std::vector<PipelineShaderDesc>& shaders) {
    std::ostringstream oss;
    for (size_t i = 0; i < shaders.size(); ++i) {
        if (i > 0) oss << "|";
        oss << GetShaderSourceKey(shaders[i].source);
    }
    return oss.str();
}

} // anonymous namespace

PipelineCache::PipelineCache(Device* device, ShaderCompiler* shader_compiler)
    : device_(device), shader_compiler_(shader_compiler) {
}

PipelineCache::~PipelineCache() = default;

ComputePipelineHandle PipelineCache::RegisterCompute(const ComputePipelineDesc& desc) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = GetShaderSourceKey(desc.source);

    // Check if already registered
    auto it = compute_shader_to_handle_.find(key);
    if (it != compute_shader_to_handle_.end()) {
        return it->second;
    }

    // Create new entry
    ComputePipelineHandle handle{next_compute_handle_++};
    ComputePipelineCacheEntry entry;
    entry.desc = desc;
    entry.needs_recompile = true;

    compute_entries_[handle] = entry;
    compute_shader_to_handle_[key] = handle;

    spdlog::debug("Registered compute pipeline: {}", key);

    return handle;
}

RasterPipelineHandle PipelineCache::RegisterRaster(const std::vector<PipelineShaderDesc>& shaders,
                                                    const RasterPipelineDesc& desc) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = GetShadersKey(shaders);

    // Check if already registered
    auto it = raster_shaders_to_handle_.find(key);
    if (it != raster_shaders_to_handle_.end()) {
        return it->second;
    }

    // Create new entry
    RasterPipelineHandle handle{next_raster_handle_++};
    RasterPipelineCacheEntry entry;
    entry.desc = desc;
    entry.shader_descs = shaders;
    entry.needs_recompile = true;

    raster_entries_[handle] = entry;
    raster_shaders_to_handle_[key] = handle;

    spdlog::debug("Registered raster pipeline: {}", key);

    return handle;
}

RtPipelineHandle PipelineCache::RegisterRayTracing(const std::vector<PipelineShaderDesc>& shaders,
                                                   const RayTracingPipelineDesc& desc) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = GetShadersKey(shaders);

    // Check if already registered
    auto it = rt_shaders_to_handle_.find(key);
    if (it != rt_shaders_to_handle_.end()) {
        return it->second;
    }

    // Create new entry
    RtPipelineHandle handle{next_rt_handle_++};
    RtPipelineCacheEntry entry;
    entry.desc = desc;
    entry.shader_descs = shaders;
    entry.needs_recompile = true;

    rt_entries_[handle] = entry;
    rt_shaders_to_handle_[key] = handle;

    spdlog::debug("Registered ray tracing pipeline: {}", key);

    return handle;
}

std::shared_ptr<ComputePipeline> PipelineCache::GetCompute(ComputePipelineHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = compute_entries_.find(handle);
    if (it == compute_entries_.end()) {
        spdlog::error("Invalid compute pipeline handle: {}", handle.index);
        return nullptr;
    }

    return it->second.pipeline;
}

std::shared_ptr<RasterPipeline> PipelineCache::GetRaster(RasterPipelineHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = raster_entries_.find(handle);
    if (it == raster_entries_.end()) {
        spdlog::error("Invalid raster pipeline handle: {}", handle.index);
        return nullptr;
    }

    return it->second.pipeline;
}

std::shared_ptr<RayTracingPipeline> PipelineCache::GetRayTracing(RtPipelineHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rt_entries_.find(handle);
    if (it == rt_entries_.end()) {
        spdlog::error("Invalid ray tracing pipeline handle: {}", handle.index);
        return nullptr;
    }

    return it->second.pipeline;
}

void PipelineCache::InvalidateStalePipelines() {
    // TODO: Implement shader file watching and invalidation
    // For now, we don't invalidate anything
}

void PipelineCache::ParallelCompileShaders() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::future<void>> tasks;

    // Compile compute shaders
    for (auto& [handle, entry] : compute_entries_) {
        if (!entry.needs_recompile || entry.is_compiling || entry.pipeline) {
            continue;
        }

        entry.is_compiling = true;

        tasks.push_back(std::async(std::launch::async, [this, handle]() {
            auto& entry = compute_entries_[handle];

            try {
                // Compile shader
                std::string profile = "cs_6_0";
                auto spirv = shader_compiler_->CompileShader(entry.desc.source, profile);

                // Create pipeline
                auto pipeline = std::make_shared<ComputePipeline>();
                *pipeline = CreateComputePipeline(device_, spirv, entry.desc);

                entry.pipeline = pipeline;
                entry.needs_recompile = false;
                entry.is_compiling = false;

                spdlog::info("Compiled compute pipeline");
            } catch (const std::exception& e) {
                spdlog::error("Failed to compile compute pipeline: {}", e.what());
                entry.is_compiling = false;
            }
        }));
    }

    // Compile raster shaders
    for (auto& [handle, entry] : raster_entries_) {
        if (!entry.needs_recompile || entry.is_compiling || entry.pipeline) {
            continue;
        }

        entry.is_compiling = true;

        tasks.push_back(std::async(std::launch::async, [this, handle]() {
            auto& entry = raster_entries_[handle];

            try {
                // Compile all shaders
                std::vector<PipelineShader<std::vector<uint8_t>>> compiled_shaders;

                for (const auto& shader_desc : entry.shader_descs) {
                    std::string profile;
                    switch (shader_desc.stage) {
                        case ShaderPipelineStage::Vertex:
                            profile = "vs_6_0";
                            break;
                        case ShaderPipelineStage::Pixel:
                            profile = "ps_6_0";
                            break;
                        default:
                            throw std::runtime_error("Invalid shader stage for raster pipeline");
                    }

                    auto spirv = shader_compiler_->CompileShader(shader_desc.source, profile);
                    compiled_shaders.push_back(PipelineShader<std::vector<uint8_t>>{spirv, shader_desc});
                }

                // Create pipeline
                auto pipeline = std::make_shared<RasterPipeline>();
                *pipeline = CreateRasterPipeline(device_, compiled_shaders, entry.desc);

                entry.pipeline = pipeline;
                entry.needs_recompile = false;
                entry.is_compiling = false;

                spdlog::info("Compiled raster pipeline");
            } catch (const std::exception& e) {
                spdlog::error("Failed to compile raster pipeline: {}", e.what());
                entry.is_compiling = false;
            }
        }));
    }

    // Compile ray tracing shaders
    for (auto& [handle, entry] : rt_entries_) {
        if (!entry.needs_recompile || entry.is_compiling || entry.pipeline) {
            continue;
        }

        entry.is_compiling = true;

        tasks.push_back(std::async(std::launch::async, [this, handle]() {
            auto& entry = rt_entries_[handle];

            try {
                // Compile all shaders
                std::vector<PipelineShader<std::vector<uint8_t>>> compiled_shaders;

                for (const auto& shader_desc : entry.shader_descs) {
                    std::string profile = "lib_6_3"; // Ray tracing uses library profile

                    auto spirv = shader_compiler_->CompileShader(shader_desc.source, profile);
                    compiled_shaders.push_back(PipelineShader<std::vector<uint8_t>>{spirv, shader_desc});
                }

                // Create pipeline
                auto pipeline = std::make_shared<RayTracingPipeline>();
                *pipeline = CreateRayTracingPipeline(device_, compiled_shaders, entry.desc);

                entry.pipeline = pipeline;
                entry.needs_recompile = false;
                entry.is_compiling = false;

                spdlog::info("Compiled ray tracing pipeline");
            } catch (const std::exception& e) {
                spdlog::error("Failed to compile ray tracing pipeline: {}", e.what());
                entry.is_compiling = false;
            }
        }));
    }

    // Wait for all compilation tasks to complete
    for (auto& task : tasks) {
        task.wait();
    }
}

void PipelineCache::PrepareFrame() {
    InvalidateStalePipelines();
    ParallelCompileShaders();
}

} // namespace tekki::backend::vulkan
