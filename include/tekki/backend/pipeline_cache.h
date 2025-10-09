```cpp
#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <future>
#include <glm/glm.hpp>
#include "tekki/core/Result.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/rust_shader_compiler.h"
#include "tekki/backend/shader_compiler.h"

namespace tekki::backend {

struct ComputePipelineHandle {
    std::size_t value;
    
    ComputePipelineHandle(std::size_t val = 0) : value(val) {}
    
    bool operator==(const ComputePipelineHandle& other) const {
        return value == other.value;
    }
    
    bool operator!=(const ComputePipelineHandle& other) const {
        return value != other.value;
    }
};

struct RasterPipelineHandle {
    std::size_t value;
    
    RasterPipelineHandle(std::size_t val = 0) : value(val) {}
    
    bool operator==(const RasterPipelineHandle& other) const {
        return value == other.value;
    }
    
    bool operator!=(const RasterPipelineHandle& other) const {
        return value != other.value;
    }
};

struct RtPipelineHandle {
    std::size_t value;
    
    RtPipelineHandle(std::size_t val = 0) : value(val) {}
    
    bool operator==(const RtPipelineHandle& other) const {
        return value == other.value;
    }
    
    bool operator!=(const RtPipelineHandle& other) const {
        return value != other.value;
    }
};

struct CompiledPipelineShaders {
    std::vector<PipelineShader<std::shared_ptr<CompiledShader>>> Shaders;
};

struct CompilePipelineShaders {
    std::vector<PipelineShaderDesc> ShaderDescs;
    
    std::future<std::shared_ptr<CompiledPipelineShaders>> Run();
};

class PipelineCache {
private:
    struct ComputePipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledShader>> LazyHandle;
        ComputePipelineDesc Desc;
        std::shared_ptr<ComputePipeline> Pipeline;
    };

    struct RasterPipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledPipelineShaders>> LazyHandle;
        RasterPipelineDesc Desc;
        std::shared_ptr<RasterPipeline> Pipeline;
    };

    struct RtPipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledPipelineShaders>> LazyHandle;
        RayTracingPipelineDesc Desc;
        std::shared_ptr<RayTracingPipeline> Pipeline;
    };

    std::shared_ptr<LazyCache> LazyCache;

    std::unordered_map<ComputePipelineHandle, ComputePipelineCacheEntry> ComputeEntries;
    std::unordered_map<RasterPipelineHandle, RasterPipelineCacheEntry> RasterEntries;
    std::unordered_map<RtPipelineHandle, RtPipelineCacheEntry> RtEntries;

    std::unordered_map<ShaderSource, ComputePipelineHandle> ComputeShaderToHandle;
    std::unordered_map<std::vector<PipelineShaderDesc>, RasterPipelineHandle> RasterShadersToHandle;
    std::unordered_map<std::vector<PipelineShaderDesc>, RtPipelineHandle> RtShadersToHandle;

    void InvalidateStalePipelines();
    
    enum class CompileTaskOutputType {
        Compute,
        Raster,
        Rt
    };
    
    struct CompileTaskOutput {
        CompileTaskOutputType Type;
        ComputePipelineHandle ComputeHandle;
        RasterPipelineHandle RasterHandle;
        RtPipelineHandle RtHandle;
        std::shared_ptr<CompiledShader> CompiledShader;
        std::shared_ptr<CompiledPipelineShaders> CompiledPipelineShaders;
    };

public:
    PipelineCache(const std::shared_ptr<LazyCache>& lazyCache);
    
    ComputePipelineHandle RegisterCompute(const ComputePipelineDesc& desc);
    
    std::shared_ptr<ComputePipeline> GetCompute(ComputePipelineHandle handle);
    
    RasterPipelineHandle RegisterRaster(const std::vector<PipelineShaderDesc>& shaders, const RasterPipelineDesc& desc);
    
    std::shared_ptr<RasterPipeline> GetRaster(RasterPipelineHandle handle);
    
    RtPipelineHandle RegisterRayTracing(const std::vector<PipelineShaderDesc>& shaders, const RayTracingPipelineDesc& desc);
    
    std::shared_ptr<RayTracingPipeline> GetRayTracing(RtPipelineHandle handle);
    
    void ParallelCompileShaders(const std::shared_ptr<vulkan::Device>& device);
    
    void PrepareFrame(const std::shared_ptr<vulkan::Device>& device);
};

} // namespace tekki::backend
```

namespace std {
    template<>
    struct hash<tekki::backend::ComputePipelineHandle> {
        std::size_t operator()(const tekki::backend::ComputePipelineHandle& handle) const {
            return std::hash<std::size_t>{}(handle.value);
        }
    };
    
    template<>
    struct hash<tekki::backend::RasterPipelineHandle> {
        std::size_t operator()(const tekki::backend::RasterPipelineHandle& handle) const {
            return std::hash<std::size_t>{}(handle.value);
        }
    };
    
    template<>
    struct hash<tekki::backend::RtPipelineHandle> {
        std::size_t operator()(const tekki::backend::RtPipelineHandle& handle) const {
            return std::hash<std::size_t>{}(handle.value);
        }
    };
    
    template<>
    struct hash<std::vector<tekki::backend::PipelineShaderDesc>> {
        std::size_t operator()(const std::vector<tekki::backend::PipelineShaderDesc>& vec) const {
            std::size_t seed = vec.size();
            for (const auto& desc : vec) {
                seed ^= std::hash<std::string>{}(desc.Entry) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}