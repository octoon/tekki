#pragma once

#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/render_graph/pass_api.h"

#include "tekki/backend/vulkan/Image.h"
#include "tekki/backend/vulkan/ray_tracing/RayTracingAcceleration.h"
#include "tekki/backend/vulkan/ray_tracing/RayTracingPipelineDesc.h"
#include "tekki/backend/vulkan/shader/ComputePipelineDesc.h"
#include "tekki/backend/vulkan/shader/PipelineShaderDesc.h"
#include "tekki/backend/vulkan/shader/ShaderPipelineStage.h"
#include "tekki/backend/vulkan/shader/ShaderSource.h"
#include "tekki/backend/dynamic_constants/DynamicConstants.h"
#include "tekki/backend/vk_sync/AccessType.h"

#include <memory>
#include <vector>

namespace tekki {
namespace render_graph {

// Forward declarations
class RenderPassApi;

// ConstBlob trait for dynamic constants
class ConstBlob {
public:
    virtual ~ConstBlob() = default;
    virtual uint32_t pushSelf(backend::DynamicConstants& dynamicConstants) = 0;
};

// Template implementation for copyable types
template<typename T>
class TypedConstBlob : public ConstBlob {
    static_assert(std::is_copy_constructible_v<T>, "T must be copyable");

public:
    explicit TypedConstBlob(T value) : m_value(std::move(value)) {}

    uint32_t pushSelf(backend::DynamicConstants& dynamicConstants) override {
        return dynamicConstants.push(&m_value);
    }

private:
    T m_value;
};

// Template implementation for vector types
template<typename T>
class VectorConstBlob : public ConstBlob {
    static_assert(std::is_copy_constructible_v<T>, "T must be copyable");

public:
    explicit VectorConstBlob(std::vector<T> values) : m_values(std::move(values)) {}

    uint32_t pushSelf(backend::DynamicConstants& dynamicConstants) override {
        return dynamicConstants.pushFromIter(m_values.begin(), m_values.end());
    }

private:
    std::vector<T> m_values;
};

// Simple render pass state
template<typename RgPipelineHandle>
class SimpleRenderPassState {
public:
    explicit SimpleRenderPassState(RgPipelineHandle pipeline)
        : m_pipeline(std::move(pipeline)) {}

    void patchConstBlobs(RenderPassApi& api);
    RenderPassPipelineBinding<RgPipelineHandle> createPipelineBinding();

    RgPipelineHandle m_pipeline;
    std::vector<RenderPassBinding> m_bindings;
    std::vector<std::pair<size_t, std::unique_ptr<ConstBlob>>> m_constBlobs;
    std::vector<std::pair<uint32_t, vk::DescriptorSet>> m_rawDescriptorSets;
};

// Simple render pass builder
template<typename RgPipelineHandle>
class SimpleRenderPass {
public:
    SimpleRenderPass(PassBuilder pass, SimpleRenderPassState<RgPipelineHandle> state)
        : m_pass(std::move(pass)), m_state(std::move(state)) {}

    // Compute pipeline constructors
    static SimpleRenderPass<RgComputePipelineHandle> newCompute(PassBuilder pass, const std::string& pipelinePath);
    static SimpleRenderPass<RgComputePipelineHandle> newComputeRust(PassBuilder pass, const std::string& entryName);

    // Ray tracing pipeline constructor
    static SimpleRenderPass<RgRtPipelineHandle> newRt(
        PassBuilder pass,
        backend::ShaderSource rgen,
        std::vector<backend::ShaderSource> miss,
        std::vector<backend::ShaderSource> hit);

    // Dispatch methods for compute
    void dispatch(const std::array<uint32_t, 3>& extent);
    void dispatchIndirect(Handle<backend::Buffer>& argsBuffer, uint64_t argsBufferOffset);

    // Trace rays methods for ray tracing
    void traceRays(Handle<backend::RayTracingAcceleration>& tlas, const std::array<uint32_t, 3>& extent);
    void traceRaysIndirect(
        Handle<backend::RayTracingAcceleration>& tlas,
        Handle<backend::Buffer>& argsBuffer,
        uint64_t argsBufferOffset);

    // Resource binding methods
    template<typename Res>
    SimpleRenderPass& read(Handle<Res>& handle);

    SimpleRenderPass& readArray(std::vector<Handle<backend::Image>>& handles);
    SimpleRenderPass& readView(Handle<backend::Image>& handle, backend::ImageViewDescBuilder viewDesc);
    SimpleRenderPass& readAspect(Handle<backend::Image>& handle, vk::ImageAspectFlags aspectMask);

    template<typename Res>
    SimpleRenderPass& write(Handle<Res>& handle);

    template<typename Res>
    SimpleRenderPass& writeNoSync(Handle<Res>& handle);

    SimpleRenderPass& writeView(Handle<backend::Image>& handle, backend::ImageViewDescBuilder viewDesc);

    // Constant binding methods
    template<typename T>
    SimpleRenderPass& constants(const T& consts);

    template<typename T>
    SimpleRenderPass& dynamicStorageBuffer(const T& consts);

    template<typename T>
    SimpleRenderPass& dynamicStorageBufferVec(const std::vector<T>& consts);

    // Raw descriptor set binding
    SimpleRenderPass& rawDescriptorSet(uint32_t setIdx, vk::DescriptorSet set);

    // Generic binding methods
    template<typename Binding>
    SimpleRenderPass& bind(const Binding& binding);

    template<typename Binding>
    SimpleRenderPass& bindMut(Binding& binding);

private:
    PassBuilder m_pass;
    SimpleRenderPassState<RgPipelineHandle> m_state;
};

// Binding traits
template<typename RgPipelineHandle>
class BindToSimpleRenderPass {
public:
    virtual ~BindToSimpleRenderPass() = default;
    virtual SimpleRenderPass<RgPipelineHandle> bind(SimpleRenderPass<RgPipelineHandle> pass) = 0;
};

template<typename RgPipelineHandle>
class BindMutToSimpleRenderPass {
public:
    virtual ~BindMutToSimpleRenderPass() = default;
    virtual SimpleRenderPass<RgPipelineHandle> bindMut(SimpleRenderPass<RgPipelineHandle> pass) = 0;
};

} // namespace render_graph
} // namespace tekki