#include "tekki/backend/pipeline_cache.h"
#include <future>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include "tekki/backend/vulkan/device.h"

namespace tekki::backend {

CompilePipelineShaders::CompilePipelineShaders(std::vector<vulkan::PipelineShaderDesc> shaderDescs)
    : ShaderDescs(std::move(shaderDescs)) {}

std::future<std::shared_ptr<CompiledPipelineShaders>> CompilePipelineShaders::Run() {
    return std::async(std::launch::async, [shaderDescs = this->ShaderDescs]() -> std::shared_ptr<CompiledPipelineShaders> {
        try {
            std::vector<std::future<std::shared_ptr<CompiledShader>>> futures;

            for (const auto& desc : shaderDescs) {
                if (desc.Source.GetType() == vulkan::ShaderSourceType::Rust) {
                    std::string entry = desc.Source.GetRustEntry();
                    futures.push_back(std::async(std::launch::async, [entry]() {
                        auto shader = CompileRustShader{entry}.Run();
                        return std::make_shared<CompiledShader>(std::move(shader));
                    }));
                } else if (desc.Source.GetType() == vulkan::ShaderSourceType::Hlsl) {
                    std::filesystem::path path = desc.Source.GetHlslPath();
                    std::string profile;
                    switch (desc.Stage) {
                        case vulkan::ShaderPipelineStage::Vertex:
                            profile = "vs";
                            break;
                        case vulkan::ShaderPipelineStage::Pixel:
                            profile = "ps";
                            break;
                        case vulkan::ShaderPipelineStage::RayGen:
                        case vulkan::ShaderPipelineStage::RayMiss:
                        case vulkan::ShaderPipelineStage::RayClosestHit:
                            profile = "lib";
                            break;
                    }
                    futures.push_back(std::async(std::launch::async, [path, profile]() {
                        auto shader = ShaderCompiler::Compile(CompileShader{path, profile});
                        return std::make_shared<CompiledShader>(std::move(shader));
                    }));
                }
            }

            std::vector<std::shared_ptr<CompiledShader>> shaders;
            for (auto& future : futures) {
                shaders.push_back(future.get());
            }

            auto result = std::make_shared<CompiledPipelineShaders>();
            for (size_t i = 0; i < shaders.size(); ++i) {
                result->Shaders.push_back(vulkan::PipelineShader<std::shared_ptr<CompiledShader>>(
                    shaders[i],
                    vulkan::PipelineShaderDesc::CreateBuilder(shaderDescs[i].Stage)
                        .SetDescriptorSetLayoutFlags(shaderDescs[i].DescriptorSetLayoutFlags)
                        .SetPushConstantsBytes(shaderDescs[i].PushConstantsBytes)
                        .SetEntry(shaderDescs[i].Entry)
                        .SetSource(shaderDescs[i].Source)
                ));
            }

            return result;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to compile pipeline shaders: ") + e.what());
        }
    });
}

PipelineCache::PipelineCache(const std::shared_ptr<backend::LazyCache>& lazyCache)
    : LazyCachePtr(lazyCache) {}

ComputePipelineHandle PipelineCache::RegisterCompute(const vulkan::ComputePipelineDesc& desc) {
    auto it = ComputeShaderToHandle.find(desc.Source);
    if (it != ComputeShaderToHandle.end()) {
        return it->second;
    }
    
    ComputePipelineHandle handle(ComputeEntries.size());
    
    std::shared_ptr<Lazy<CompiledShader>> compileTask;
    if (desc.Source.GetType() == vulkan::ShaderSourceType::Rust) {
        std::string entry = desc.Source.GetRustEntry();
        auto future = std::async(std::launch::async, [entry]() {
            auto shader = CompileRustShader{entry}.Run();
            return std::make_shared<CompiledShader>(std::move(shader));
        }).share();
        compileTask = std::make_shared<Lazy<CompiledShader>>(future);
    } else if (desc.Source.GetType() == vulkan::ShaderSourceType::Hlsl) {
        std::filesystem::path path = desc.Source.GetHlslPath();
        auto future = std::async(std::launch::async, [path]() {
            auto shader = ShaderCompiler::Compile(CompileShader{path, "cs"});
            return std::make_shared<CompiledShader>(std::move(shader));
        }).share();
        compileTask = std::make_shared<Lazy<CompiledShader>>(future);
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

std::shared_ptr<vulkan::ComputePipeline> PipelineCache::GetCompute(ComputePipelineHandle handle) {
    auto it = ComputeEntries.find(handle);
    if (it == ComputeEntries.end()) {
        throw std::invalid_argument("Invalid compute pipeline handle");
    }
    if (!it->second.Pipeline) {
        throw std::runtime_error("Compute pipeline not yet compiled");
    }
    return it->second.Pipeline;
}

RasterPipelineHandle PipelineCache::RegisterRaster(const std::vector<vulkan::PipelineShaderDesc>& shaders, const vulkan::RasterPipelineDesc& desc) {
    auto it = RasterShadersToHandle.find(shaders);
    if (it != RasterShadersToHandle.end()) {
        return it->second;
    }
    
    RasterPipelineHandle handle(RasterEntries.size());

    auto future = std::async(std::launch::async, [shaders]() {
        return CompilePipelineShaders{shaders}.Run().get();
    }).share();
    auto compileTask = std::make_shared<Lazy<CompiledPipelineShaders>>(future);

    RasterEntries[handle] = RasterPipelineCacheEntry{
        compileTask,
        desc,
        nullptr
    };
    
    RasterShadersToHandle[shaders] = handle;
    return handle;
}

std::shared_ptr<vulkan::RasterPipeline> PipelineCache::GetRaster(RasterPipelineHandle handle) {
    auto it = RasterEntries.find(handle);
    if (it == RasterEntries.end()) {
        throw std::invalid_argument("Invalid raster pipeline handle");
    }
    if (!it->second.Pipeline) {
        throw std::runtime_error("Raster pipeline not yet compiled");
    }
    return it->second.Pipeline;
}

RtPipelineHandle PipelineCache::RegisterRayTracing(const std::vector<vulkan::PipelineShaderDesc>& shaders, const vulkan::RayTracingPipelineDesc& desc) {
    auto it = RtShadersToHandle.find(shaders);
    if (it != RtShadersToHandle.end()) {
        return it->second;
    }
    
    RtPipelineHandle handle(RtEntries.size());

    auto future = std::async(std::launch::async, [shaders]() {
        return CompilePipelineShaders{shaders}.Run().get();
    }).share();
    auto compileTask = std::make_shared<Lazy<CompiledPipelineShaders>>(future);

    RtEntries[handle] = RtPipelineCacheEntry{
        compileTask,
        desc,
        nullptr
    };
    
    RtShadersToHandle[shaders] = handle;
    return handle;
}

std::shared_ptr<vulkan::RayTracingPipeline> PipelineCache::GetRayTracing(RtPipelineHandle handle) {
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
    // TODO: Implement stale pipeline detection
    // The Rust version uses turbosloth::Lazy::is_stale() to detect if shader sources have changed
    // For now, we skip this check since hot-reloading is not yet implemented

    // for (auto& entry : ComputeEntries) {
    //     if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
    //         entry.second.Pipeline = nullptr;
    //     }
    // }

    // for (auto& entry : RasterEntries) {
    //     if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
    //         entry.second.Pipeline = nullptr;
    //     }
    // }

    // for (auto& entry : RtEntries) {
    //     if (entry.second.Pipeline && entry.second.LazyHandle->IsStale()) {
    //         entry.second.Pipeline = nullptr;
    //     }
    // }
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
            } catch (...) {
                throw;
            }
        }
        
        for (const auto& result : results) {
            switch (result.Type) {
                case CompileTaskOutputType::Compute: {
                    auto& entry = ComputeEntries.at(result.ComputeHandle);
                    try {
                        // Convert from std::vector<uint8_t> to std::vector<uint32_t> if needed
                        // CreateComputePipeline expects std::vector<uint8_t>
                        entry.Pipeline = std::make_shared<vulkan::ComputePipeline>(
                            vulkan::CreateComputePipeline(device.get(), result.CompiledShader->Spirv, entry.Desc)
                        );
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to create compute pipeline: ") + e.what());
                    }
                    break;
                }
                case CompileTaskOutputType::Raster: {
                    auto& entry = RasterEntries.at(result.RasterHandle);
                    try {
                        std::vector<vulkan::PipelineShader<std::vector<uint8_t>>> compiledShaders;
                        for (const auto& shader : result.CompiledPipelineShaders->Shaders) {
                            compiledShaders.push_back(vulkan::PipelineShader<std::vector<uint8_t>>(
                                shader.Code->Spirv,
                                vulkan::PipelineShaderDesc::CreateBuilder(shader.Desc.Stage)
                                    .SetDescriptorSetLayoutFlags(shader.Desc.DescriptorSetLayoutFlags)
                                    .SetPushConstantsBytes(shader.Desc.PushConstantsBytes)
                                    .SetEntry(shader.Desc.Entry)
                                    .SetSource(shader.Desc.Source)
                            ));
                        }
                        entry.Pipeline = std::make_shared<vulkan::RasterPipeline>(
                            vulkan::CreateRasterPipeline(device.get(), compiledShaders, entry.Desc)
                        );
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to create raster pipeline: ") + e.what());
                    }
                    break;
                }
                case CompileTaskOutputType::Rt: {
                    auto& entry = RtEntries.at(result.RtHandle);
                    try {
                        std::vector<vulkan::PipelineShader<std::vector<uint32_t>>> compiledShaders;
                        for (const auto& shader : result.CompiledPipelineShaders->Shaders) {
                            // Convert from std::vector<uint8_t> to std::vector<uint32_t>
                            const auto& spirv = shader.Code->Spirv;
                            std::vector<uint32_t> spirv32(spirv.size() / 4);
                            std::memcpy(spirv32.data(), spirv.data(), spirv.size());

                            compiledShaders.push_back(vulkan::PipelineShader<std::vector<uint32_t>>(
                                spirv32,
                                vulkan::PipelineShaderDesc::CreateBuilder(shader.Desc.Stage)
                                    .SetDescriptorSetLayoutFlags(shader.Desc.DescriptorSetLayoutFlags)
                                    .SetPushConstantsBytes(shader.Desc.PushConstantsBytes)
                                    .SetEntry(shader.Desc.Entry)
                                    .SetSource(shader.Desc.Source)
                            ));
                        }
                        entry.Pipeline = vulkan::CreateRayTracingPipeline(device, compiledShaders, entry.Desc);
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