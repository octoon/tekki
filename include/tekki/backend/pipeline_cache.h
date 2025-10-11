#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <future>
#include <functional>
#include <thread>
#include <chrono>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/rust_shader_compiler.h"
#include "tekki/backend/shader_compiler.h"

namespace tekki::backend {

// Forward declarations
namespace vulkan {
    class Device;
}

// Simple lazy evaluation wrapper using std::shared_future
template<typename T>
class Lazy {
private:
    std::shared_future<std::shared_ptr<T>> Future;

public:
    Lazy() = default;

    explicit Lazy(std::shared_future<std::shared_ptr<T>> future)
        : Future(std::move(future)) {}

    std::shared_ptr<T> Get() const {
        if (Future.valid()) {
            return Future.get();
        }
        return nullptr;
    }

    bool IsReady() const {
        return Future.valid() &&
               Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
};

// LazyCache manages lazy evaluation tasks
class LazyCache {
public:
    LazyCache() = default;

    template<typename T, typename Func>
    Lazy<T> GetOrInsert(const std::string& key, Func&& func) {
        // For now, just execute the function
        // In a full implementation, this would cache results
        auto promise = std::make_shared<std::promise<std::shared_ptr<T>>>();
        auto future = promise->get_future().share();

        // Execute async
        std::thread([promise, func = std::forward<Func>(func)]() mutable {
            try {
                promise->set_value(std::make_shared<T>(func()));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();

        return Lazy<T>(future);
    }
};

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

} // namespace tekki::backend

// Hash specializations MUST come before any use of unordered_map with these types
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
    struct hash<std::vector<tekki::backend::vulkan::PipelineShaderDesc>> {
        std::size_t operator()(const std::vector<tekki::backend::vulkan::PipelineShaderDesc>& vec) const {
            std::size_t seed = vec.size();
            for (const auto& desc : vec) {
                seed ^= std::hash<std::string>{}(desc.Entry) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

namespace tekki::backend {

struct CompiledPipelineShaders {
    std::vector<vulkan::PipelineShader<std::shared_ptr<CompiledShader>>> Shaders;
};

struct CompilePipelineShaders {
    std::vector<vulkan::PipelineShaderDesc> ShaderDescs;

    explicit CompilePipelineShaders(std::vector<vulkan::PipelineShaderDesc> shaderDescs);
    std::future<std::shared_ptr<CompiledPipelineShaders>> Run();
};

class PipelineCache {
private:
    struct ComputePipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledShader>> LazyHandle;
        vulkan::ComputePipelineDesc Desc;
        std::shared_ptr<vulkan::ComputePipeline> Pipeline;
    };

    struct RasterPipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledPipelineShaders>> LazyHandle;
        vulkan::RasterPipelineDesc Desc;
        std::shared_ptr<vulkan::RasterPipeline> Pipeline;
    };

    struct RtPipelineCacheEntry {
        std::shared_ptr<Lazy<CompiledPipelineShaders>> LazyHandle;
        vulkan::RayTracingPipelineDesc Desc;
        std::shared_ptr<vulkan::RayTracingPipeline> Pipeline;
    };

    std::shared_ptr<backend::LazyCache> LazyCachePtr;

    std::unordered_map<ComputePipelineHandle, ComputePipelineCacheEntry> ComputeEntries;
    std::unordered_map<RasterPipelineHandle, RasterPipelineCacheEntry> RasterEntries;
    std::unordered_map<RtPipelineHandle, RtPipelineCacheEntry> RtEntries;

    std::unordered_map<vulkan::ShaderSource, ComputePipelineHandle> ComputeShaderToHandle;
    std::unordered_map<std::vector<vulkan::PipelineShaderDesc>, RasterPipelineHandle> RasterShadersToHandle;
    std::unordered_map<std::vector<vulkan::PipelineShaderDesc>, RtPipelineHandle> RtShadersToHandle;

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
    PipelineCache(const std::shared_ptr<backend::LazyCache>& lazyCache);

    ComputePipelineHandle RegisterCompute(const vulkan::ComputePipelineDesc& desc);

    std::shared_ptr<vulkan::ComputePipeline> GetCompute(ComputePipelineHandle handle);

    RasterPipelineHandle RegisterRaster(const std::vector<vulkan::PipelineShaderDesc>& shaders, const vulkan::RasterPipelineDesc& desc);

    std::shared_ptr<vulkan::RasterPipeline> GetRaster(RasterPipelineHandle handle);

    RtPipelineHandle RegisterRayTracing(const std::vector<vulkan::PipelineShaderDesc>& shaders, const vulkan::RayTracingPipelineDesc& desc);

    std::shared_ptr<vulkan::RayTracingPipeline> GetRayTracing(RtPipelineHandle handle);
    
    void ParallelCompileShaders(const std::shared_ptr<vulkan::Device>& device);
    
    void PrepareFrame(const std::shared_ptr<vulkan::Device>& device);
};

} // namespace tekki::backend