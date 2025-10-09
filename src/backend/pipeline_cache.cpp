#include "tekki/backend/pipeline_cache.h"
#include <future>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include "tekki/backend/vulkan/device.h"

namespace tekki::backend {

CompilePipelineShaders::CompilePipelineShaders(std::vector<PipelineShaderDesc> shaderDescs)
    : ShaderDescs(std::move(shaderDescs)) {}

std::future<std::shared_ptr<CompiledPipelineShaders>> CompilePipelineShaders::Run() {
    return std::async(std::launch::async, [this]() -> std::shared_ptr<CompiledPipelineShaders> {
        try {
            std::vector<std::future<std::shared_ptr<CompiledShader>>> futures;
            
            for (const auto& desc : ShaderDescs) {
                if (std::holds_alternative<ShaderSource::Rust>(desc.Source)) {
                    const auto& rustSource = std::get<ShaderSource::Rust>(desc.Source);
                    futures.push_back(std::async(std::launch::async, [entry = rustSource.Entry]() {
                        return CompileRustShader{entry}.Run();
                    }));
                } else if (std::holds_alternative<ShaderSource::Hlsl>(desc.Source)) {
                    const auto& hlslSource = std::get<ShaderSource::Hlsl>(desc.Source);
                    std::string profile;
                    switch (desc.Stage) {
                        case ShaderPipelineStage::Vertex:
                            profile = "vs";
                            break;
                        case ShaderPipelineStage::Pixel:
                            profile = "ps";
                            break;
                        case ShaderPipelineStage::RayGen:
                        case ShaderPipelineStage::RayMiss:
                        case ShaderPipelineStage::RayClosestHit:
                            profile = "lib";
                            break;
                    }
                    futures.push_back(std::async(std::launch::async, [path = hlslSource.Path, profile]() {
                        return CompileShader{path, profile}.Run();
                    }));
                }
            }
            
            std::vector<std::shared_ptr<CompiledShader>> shaders;
            for (auto& future : futures) {
                shaders.push_back(future.get());
            }
            
            auto result = std::make_shared<CompiledPipelineShaders>();
            for (size_t i = 0; i < shaders.size(); ++i) {
                result->Shaders.push_back(PipelineShader<std::shared_ptr<CompiledShader>>{
                    shaders[i],
                    ShaderDescs[i]
                });
            }
            
            return result;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to compile pipeline shaders: ") + e.what());
        }
    });
}

PipelineCache::PipelineCache(const std::shared_ptr<LazyCache>& lazyCache)
    : LazyCache(lazyCache) {}

ComputePipelineHandle PipelineCache::RegisterCompute(const ComputePipelineDesc& desc) {
    auto it = ComputeShaderToHandle.find(desc.Source);
    if (it != ComputeShaderToHandle.end()) {
        return it->second;
    }
    
    ComputePipelineHandle handle(ComputeEntries.size());
    
    std::shared_ptr<Lazy<CompiledShader>> compileTask;
    if (std::holds_alternative<ShaderSource::Rust>(desc.Source)) {
        const auto& rustSource = std::get<ShaderSource::Rust>(desc.Source);
        compileTask = std::make_shared<Lazy<CompiledShader>>(
            [entry = rustSource.Entry]() { return CompileRustShader{entry}.Run(); }
        );
    } else if (std::holds_alternative<ShaderSource::Hlsl>(desc.Source)) {
        const auto& hlslSource = std::get<ShaderSource::Hlsl>(desc.Source);
        compileTask = std::make_shared<Lazy<CompiledShader>>(
            [path = hlslSource.Path]() { return CompileShader{path, "cs"}.Run(); }
        );
    } else {
        throw std::invalid_argument("Unsupported shader source type");
    }
    
    ComputeEntries[handle] = ComputePipelineCacheEntry{
        compileTask,
        desc,
        nullptr
    };
    
    ComputeShaderToHandle[desc.Source] = handle;
    return handle;
}

std::shared_ptr<ComputePipeline> PipelineCache::GetCompute(ComputePipelineHandle handle) {
    auto it = ComputeEntries.find(handle);
    if (it == ComputeEntries.end()) {
        throw std::invalid_argument("Invalid compute pipeline handle");
    }
    if (!it->second.Pipeline) {
        throw std::runtime_error("Compute pipeline not yet compiled");
    }
    return it->second.Pipeline;
}

RasterPipelineHandle PipelineCache::RegisterRaster(const std::vector<PipelineShaderDesc>& shaders, const RasterPipelineDesc& desc) {
    auto it = RasterShadersToHandle.find(shaders);
    if (it != RasterShadersToHandle.end()) {
        return it->second;
    }
    
    RasterPipelineHandle handle(RasterEntries.size());
    
    auto compileTask = std::make_shared<Lazy<CompiledPipelineShaders>>(
        [shaders]() { return CompilePipelineShaders{shaders}.Run().get(); }
    );
    
    RasterEntries[handle] = RasterPipelineCacheEntry{
        compileTask,
        desc,
        nullptr
    };
    
    RasterShadersToHandle[shaders] = handle;
    return handle;
}

std::shared_ptr<RasterPipeline> PipelineCache::GetRaster(RasterPipelineHandle handle) {
    auto it = RasterEntries.find(handle);
    if (it == RasterEntries.end()) {
        throw std::invalid_argument("Invalid raster pipeline handle");
    }
    if (!it->second.Pipeline) {
        throw std::runtime_error("Raster pipeline not yet compiled");
    }
    return it->second.Pipeline;
}

RtPipelineHandle PipelineCache::RegisterRayTracing(const std::vector<PipelineShaderDesc>& shaders, const RayTracingPipelineDesc& desc) {
    auto it = RtShadersToHandle.find(shaders);
    if (it != RtShadersToHandle.end()) {
        return it->second;
    }
    
    RtPipelineHandle handle(RtEntries.size());
    
    auto compileTask = std::make_shared<Lazy<CompiledPipelineShaders>>(
        [shaders]() { return CompilePipelineShaders{shaders}.Run().get(); }
    );
    
    RtEntries[handle] = RtPipelineCacheEntry{
        compileTask,
        desc,
        nullptr
    };
    
    RtShadersToHandle[shaders] = handle;
    return handle;
}

std::shared_ptr<RayTracingPipeline> PipelineCache::GetRayTracing(RtPipelineHandle handle) {
    auto it = RtEntries.find(handle);
    if (it == RtEntries.end()) {
        throw std::invalid_argument("Invalid ray tracing pipeline handle");
    }
    if (!it->second.Pipeline) {
        throw std::runtime_error("Ray tracing pipeline not yet compiled");
    }
    return it->second.Pipeline;
}

void PipelineCache::InvalidateStalePipelines() {
    for (auto& entry : ComputeEntries) {
        if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
            entry.second.Pipeline = nullptr;
        }
    }
    
    for (auto& entry : RasterEntries) {
        if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
            entry.second.Pipeline = nullptr;
        }
    }
    
    for (auto& entry : RtEntries) {
        if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
            entry.second.Pipeline = nullptr;
        }
    }
}

void PipelineCache::ParallelCompileShaders(const std::shared_ptr<vulkan::Device>& device) {
    std::vector<std::future<CompileTaskOutput>> tasks;
    
    for (auto& entry : ComputeEntries) {
        if (!entry.second.Pipeline) {
            auto handle = entry.first;
            auto lazyHandle = entry.second.LazyHandle;
            tasks.push_back(std::async(std::launch::async, [handle, lazyHandle]() {
                try {
                    auto compiled = lazyHandle->Get();
                    return CompileTaskOutput{
                        CompileTaskOutputType::Compute,
                        handle,
                        RasterPipelineHandle(),
                        RtPipelineHandle(),
                        compiled,
                        nullptr
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Failed to compile compute shader: ") + e.what());
                }
            }));
        }
    }
    
    for (auto& entry : RasterEntries) {
        if (!entry.second.Pipeline) {
            auto handle = entry.first;
            auto lazyHandle = entry.second.LazyHandle;
            tasks.push_back(std::async(std::launch::async, [handle, lazyHandle]() {
                try {
                    auto compiled = lazyHandle->Get();
                    return CompileTaskOutput{
                        CompileTaskOutputType::Raster,
                        ComputePipelineHandle(),
                        handle,
                        RtPipelineHandle(),
                        nullptr,
                        compiled
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Failed to compile raster shaders: ") + e.what());
                }
            }));
        }
    }
    
    for (auto& entry : RtEntries) {
        if (!entry.second.Pipeline) {
            auto handle = entry.first;
            auto lazyHandle = entry.second.LazyHandle;
            tasks.push_back(std::async(std::launch::async, [handle, lazyHandle]() {
                try {
                    auto compiled = lazyHandle->Get();
                    return CompileTaskOutput{
                        CompileTaskOutputType::Rt,
                        ComputePipelineHandle(),
                        RasterPipelineHandle(),
                        handle,
                        nullptr,
                        compiled
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Failed to compile ray tracing shaders: ") + e.what());
                }
            }));
        }
    }
    
    if (!tasks.empty()) {
        std::vector<CompileTaskOutput> results;
        for (auto& task : tasks) {
            try {
                results.push_back(task.get());
            } catch (const std::exception& e) {
                throw;
            }
        }
        
        for (const auto& result : results) {
            switch (result.Type) {
                case CompileTaskOutputType::Compute: {
                    auto& entry = ComputeEntries.at(result.ComputeHandle);
                    try {
                        entry.Pipeline = std::make_shared<ComputePipeline>(
                            CreateComputePipeline(device.get(), result.CompiledShader->Spirv, entry.Desc)
                        );
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to create compute pipeline: ") + e.what());
                    }
                    break;
                }
                case CompileTaskOutputType::Raster: {
                    auto& entry = RasterEntries.at(result.RasterHandle);
                    try {
                        std::vector<PipelineShader<std::vector<uint32_t>>> compiledShaders;
                        for (const auto& shader : result.CompiledPipelineShaders->Shaders) {
                            compiledShaders.push_back(PipelineShader<std::vector<uint32_t>>{
                                shader.Code->Spirv,
                                shader.Desc
                            });
                        }
                        entry.Pipeline = std::make_shared<RasterPipeline>(
                            CreateRasterPipeline(device.get(), compiledShaders, entry.Desc)
                        );
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to create raster pipeline: ") + e.what());
                    }
                    break;
                }
                case CompileTaskOutputType::Rt: {
                    auto& entry = RtEntries.at(result.RtHandle);
                    try {
                        std::vector<PipelineShader<std::vector<uint32_t>>> compiledShaders;
                        for (const auto& shader : result.CompiledPipelineShaders->Shaders) {
                            compiledShaders.push_back(PipelineShader<std::vector<uint32_t>>{
                                shader.Code->Spirv,
                                shader.Desc
                            });
                        }
                        entry.Pipeline = std::make_shared<RayTracingPipeline>(
                            CreateRayTracingPipeline(device.get(), compiledShaders, entry.Desc)
                        );
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to create ray tracing pipeline: ") + e.what());
                    }
                    break;
                }
            }
        }
    }
}

void PipelineCache::PrepareFrame(const std::shared_ptr<vulkan::Device>& device) {
    InvalidateStalePipelines();
    ParallelCompileShaders(device);
}

} // namespace tekki::backend