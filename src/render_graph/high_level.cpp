#include "tekki/render_graph/high_level.h"

namespace tekki {
namespace render_graph {

// SimpleRenderPassState implementation
template<typename RgPipelineHandle>
void SimpleRenderPassState<RgPipelineHandle>::patchConstBlobs(RenderPassApi& api) {
    auto& dynamicConstants = api.dynamicConstants();

    auto constBlobs = std::move(m_constBlobs);
    for (auto& [bindingIdx, blob] : constBlobs) {
        uint32_t dynamicConstantsOffset = blob->pushSelf(dynamicConstants);

        auto& binding = m_bindings[bindingIdx];
        if (std::holds_alternative<RenderPassBinding::DynamicConstants>(binding)) {
            std::get<RenderPassBinding::DynamicConstants>(binding) = dynamicConstantsOffset;
        } else if (std::holds_alternative<RenderPassBinding::DynamicConstantsStorageBuffer>(binding)) {
            std::get<RenderPassBinding::DynamicConstantsStorageBuffer>(binding) = dynamicConstantsOffset;
        } else {
            assert(false && "Unexpected binding type for const blob");
        }
    }
}

template<typename RgPipelineHandle>
RenderPassPipelineBinding<RgPipelineHandle> SimpleRenderPassState<RgPipelineHandle>::createPipelineBinding() {
    auto binding = m_pipeline.intoBinding().descriptorSet(0, m_bindings);

    for (auto& [setIdx, descriptorSet] : m_rawDescriptorSets) {
        binding = binding.rawDescriptorSet(setIdx, descriptorSet);
    }

    return binding;
}

// SimpleRenderPass implementation
SimpleRenderPass<RgComputePipelineHandle> SimpleRenderPass<RgComputePipelineHandle>::newCompute(
    PassBuilder pass, const std::string& pipelinePath) {

    auto pipeline = pass.registerComputePipeline(pipelinePath);
    return SimpleRenderPass<RgComputePipelineHandle>(
        std::move(pass),
        SimpleRenderPassState<RgComputePipelineHandle>(std::move(pipeline))
    );
}

SimpleRenderPass<RgComputePipelineHandle> SimpleRenderPass<RgComputePipelineHandle>::newComputeRust(
    PassBuilder pass, const std::string& entryName) {

    auto pipelineDesc = backend::ComputePipelineDesc::builder()
        .computeRust(entryName)
        .build();

    auto pipeline = pass.registerComputePipelineWithDesc(std::move(pipelineDesc));
    return SimpleRenderPass<RgComputePipelineHandle>(
        std::move(pass),
        SimpleRenderPassState<RgComputePipelineHandle>(std::move(pipeline))
    );
}

SimpleRenderPass<RgRtPipelineHandle> SimpleRenderPass<RgRtPipelineHandle>::newRt(
    PassBuilder pass,
    backend::ShaderSource rgen,
    std::vector<backend::ShaderSource> miss,
    std::vector<backend::ShaderSource> hit) {

    std::vector<backend::PipelineShaderDesc> shaders;
    shaders.reserve(1 + miss.size() + hit.size());

    // Ray generation shader
    shaders.push_back(
        backend::PipelineShaderDesc::builder(backend::ShaderPipelineStage::RayGen)
            .source(std::move(rgen))
            .build()
    );

    // Miss shaders
    for (auto& source : miss) {
        shaders.push_back(
            backend::PipelineShaderDesc::builder(backend::ShaderPipelineStage::RayMiss)
                .source(std::move(source))
                .build()
        );
    }

    // Hit shaders
    for (auto& source : hit) {
        shaders.push_back(
            backend::PipelineShaderDesc::builder(backend::ShaderPipelineStage::RayClosestHit)
                .source(std::move(source))
                .build()
        );
    }

    auto pipeline = pass.registerRayTracingPipeline(
        shaders,
        backend::RayTracingPipelineDesc::default_().maxPipelineRayRecursionDepth(1)
    );

    return SimpleRenderPass<RgRtPipelineHandle>(
        std::move(pass),
        SimpleRenderPassState<RgRtPipelineHandle>(std::move(pipeline))
    );
}

void SimpleRenderPass<RgComputePipelineHandle>::dispatch(const std::array<uint32_t, 3>& extent) {
    auto state = std::move(m_state);

    m_pass.render([state = std::move(state), extent](RenderPassApi& api) mutable -> Result<void> {
        state.patchConstBlobs(api);

        auto pipeline = api.bindComputePipeline(state.createPipelineBinding())?;
        pipeline.dispatch(extent);

        return {};
    });
}

void SimpleRenderPass<RgComputePipelineHandle>::dispatchIndirect(
    Handle<backend::Buffer>& argsBuffer, uint64_t argsBufferOffset) {

    auto argsBufferRef = m_pass.read(argsBuffer, backend::AccessType::IndirectBuffer);
    auto state = std::move(m_state);

    m_pass.render([state = std::move(state), argsBufferRef = std::move(argsBufferRef), argsBufferOffset](RenderPassApi& api) mutable -> Result<void> {
        state.patchConstBlobs(api);

        auto pipeline = api.bindComputePipeline(state.createPipelineBinding())?;
        pipeline.dispatchIndirect(argsBufferRef, argsBufferOffset);

        return {};
    });
}

void SimpleRenderPass<RgRtPipelineHandle>::traceRays(
    Handle<backend::RayTracingAcceleration>& tlas, const std::array<uint32_t, 3>& extent) {

    auto tlasRef = m_pass.read(tlas, backend::AccessType::AnyShaderReadOther);
    auto state = std::move(m_state);

    m_pass.render([state = std::move(state), tlasRef = std::move(tlasRef), extent](RenderPassApi& api) mutable -> Result<void> {
        state.patchConstBlobs(api);

        auto pipeline = api.bindRayTracingPipeline(
            state.createPipelineBinding().descriptorSet(3, {tlasRef.bind()})
        )?;
        pipeline.traceRays(extent);

        return {};
    });
}

void SimpleRenderPass<RgRtPipelineHandle>::traceRaysIndirect(
    Handle<backend::RayTracingAcceleration>& tlas,
    Handle<backend::Buffer>& argsBuffer,
    uint64_t argsBufferOffset) {

    auto argsBufferRef = m_pass.read(argsBuffer, backend::AccessType::IndirectBuffer);
    auto tlasRef = m_pass.read(tlas, backend::AccessType::AnyShaderReadOther);
    auto state = std::move(m_state);

    m_pass.render([state = std::move(state), tlasRef = std::move(tlasRef), argsBufferRef = std::move(argsBufferRef), argsBufferOffset](RenderPassApi& api) mutable -> Result<void> {
        state.patchConstBlobs(api);

        auto pipeline = api.bindRayTracingPipeline(
            state.createPipelineBinding().descriptorSet(3, {tlasRef.bind()})
        )?;
        pipeline.traceRaysIndirect(argsBufferRef, argsBufferOffset);

        return {};
    });
}

// Resource binding methods
template<typename RgPipelineHandle>
template<typename Res>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::read(Handle<Res>& handle) {
    auto handleRef = m_pass.read(handle, backend::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
    m_state.m_bindings.push_back(handleRef.bind());
    return *this;
}

template<typename RgPipelineHandle>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::readArray(std::vector<Handle<backend::Image>>& handles) {
    assert(!handles.empty());

    std::vector<Ref<backend::Image, GpuSrv>> handleRefs;
    handleRefs.reserve(handles.size());

    for (auto& handle : handles) {
        handleRefs.push_back(m_pass.read(handle, backend::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer));
    }

    m_state.m_bindings.push_back(BindRgRef::bind(handleRefs));
    return *this;
}

template<typename RgPipelineHandle>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::readView(
    Handle<backend::Image>& handle, backend::ImageViewDescBuilder viewDesc) {

    auto handleRef = m_pass.read(handle, backend::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
    m_state.m_bindings.push_back(handleRef.bindView(std::move(viewDesc)));
    return *this;
}

template<typename RgPipelineHandle>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::readAspect(
    Handle<backend::Image>& handle, vk::ImageAspectFlags aspectMask) {

    auto handleRef = m_pass.read(handle, backend::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
    auto viewDesc = backend::ImageViewDescBuilder::default_().aspectMask(aspectMask);
    m_state.m_bindings.push_back(handleRef.bindView(std::move(viewDesc)));
    return *this;
}

template<typename RgPipelineHandle>
template<typename Res>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::write(Handle<Res>& handle) {
    auto handleRef = m_pass.write(handle, backend::AccessType::AnyShaderWrite);
    m_state.m_bindings.push_back(handleRef.bind());
    return *this;
}

template<typename RgPipelineHandle>
template<typename Res>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::writeNoSync(Handle<Res>& handle) {
    auto handleRef = m_pass.writeNoSync(handle, backend::AccessType::AnyShaderWrite);
    m_state.m_bindings.push_back(handleRef.bind());
    return *this;
}

template<typename RgPipelineHandle>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::writeView(
    Handle<backend::Image>& handle, backend::ImageViewDescBuilder viewDesc) {

    auto handleRef = m_pass.write(handle, backend::AccessType::AnyShaderWrite);
    m_state.m_bindings.push_back(handleRef.bindView(std::move(viewDesc)));
    return *this;
}

// Constant binding methods
template<typename RgPipelineHandle>
template<typename T>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::constants(const T& consts) {
    size_t bindingIdx = m_state.m_bindings.size();
    m_state.m_bindings.push_back(RenderPassBinding::DynamicConstants(0));
    m_state.m_constBlobs.emplace_back(bindingIdx, std::make_unique<TypedConstBlob<T>>(consts));
    return *this;
}

template<typename RgPipelineHandle>
template<typename T>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::dynamicStorageBuffer(const T& consts) {
    size_t bindingIdx = m_state.m_bindings.size();
    m_state.m_bindings.push_back(RenderPassBinding::DynamicConstantsStorageBuffer(0));
    m_state.m_constBlobs.emplace_back(bindingIdx, std::make_unique<TypedConstBlob<T>>(consts));
    return *this;
}

template<typename RgPipelineHandle>
template<typename T>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::dynamicStorageBufferVec(const std::vector<T>& consts) {
    size_t bindingIdx = m_state.m_bindings.size();
    m_state.m_bindings.push_back(RenderPassBinding::DynamicConstantsStorageBuffer(0));
    m_state.m_constBlobs.emplace_back(bindingIdx, std::make_unique<VectorConstBlob<T>>(consts));
    return *this;
}

// Raw descriptor set binding
template<typename RgPipelineHandle>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::rawDescriptorSet(uint32_t setIdx, vk::DescriptorSet set) {
    m_state.m_rawDescriptorSets.emplace_back(setIdx, set);
    return *this;
}

// Generic binding methods
template<typename RgPipelineHandle>
template<typename Binding>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::bind(const Binding& binding) {
    return binding.bind(*this);
}

template<typename RgPipelineHandle>
template<typename Binding>
SimpleRenderPass<RgPipelineHandle>& SimpleRenderPass<RgPipelineHandle>::bindMut(Binding& binding) {
    return binding.bindMut(*this);
}

// Explicit template instantiations
template class SimpleRenderPass<RgComputePipelineHandle>;
template class SimpleRenderPass<RgRtPipelineHandle>;

template class SimpleRenderPassState<RgComputePipelineHandle>;
template class SimpleRenderPassState<RgRtPipelineHandle>;

} // namespace render_graph
} // namespace tekki