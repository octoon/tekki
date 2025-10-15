#include "tekki/render_graph/simple_render_pass.h"
#include <stdexcept>

namespace tekki::render_graph {

SimpleRenderPass::SimpleRenderPass(PassBuilder&& pass)
    : pass_(std::move(pass)) {
}

SimpleRenderPass SimpleRenderPass::NewCompute(
    PassBuilder& pass,
    const std::string& pipeline_path
) {
    // Register compute pipeline
    auto pipeline = pass.RegisterComputePipeline(pipeline_path);
    return SimpleRenderPass(std::move(pass));
}

SimpleRenderPass SimpleRenderPass::NewComputeRust(
    PassBuilder& pass,
    const std::string& entry_name
) {
    // Register Rust compute pipeline
    tekki::backend::vulkan::ComputePipelineDesc desc;
    // TODO: Implement Rust shader pipeline registration
    auto pipeline = pass.RegisterComputePipelineWithDesc(desc);
    return SimpleRenderPass(std::move(pass));
}

SimpleRenderPass SimpleRenderPass::NewRt(
    PassBuilder& pass,
    const tekki::backend::vulkan::ShaderSource& rgen,
    const std::vector<tekki::backend::vulkan::ShaderSource>& miss,
    const std::vector<tekki::backend::vulkan::ShaderSource>& hit
) {
    // Register ray tracing pipeline
    std::vector<tekki::backend::vulkan::PipelineShaderDesc> shaders;

    // Add ray generation shader
    tekki::backend::vulkan::PipelineShaderDesc rgen_desc;
    rgen_desc.Stage = tekki::backend::vulkan::ShaderPipelineStage::RayGen;
    rgen_desc.Source = rgen;
    shaders.push_back(rgen_desc);

    // Add miss shaders
    for (const auto& miss_source : miss) {
        tekki::backend::vulkan::PipelineShaderDesc miss_desc;
        miss_desc.Stage = tekki::backend::vulkan::ShaderPipelineStage::RayMiss;
        miss_desc.Source = miss_source;
        shaders.push_back(miss_desc);
    }

    // Add hit shaders
    for (const auto& hit_source : hit) {
        tekki::backend::vulkan::PipelineShaderDesc hit_desc;
        hit_desc.Stage = tekki::backend::vulkan::ShaderPipelineStage::RayClosestHit;
        hit_desc.Source = hit_source;
        shaders.push_back(hit_desc);
    }

    tekki::backend::vulkan::RayTracingPipelineDesc rt_desc;
    rt_desc.MaxPipelineRayRecursionDepth = 1;

    auto pipeline = pass.RegisterRayTracingPipeline(shaders, rt_desc);
    return SimpleRenderPass(std::move(pass));
}

void SimpleRenderPass::Dispatch(const glm::u32vec3& extent) {
    // For now, just call render with a basic dispatch
    pass_.Render([extent](RenderPassApi& api) {
        // TODO: Implement proper dispatch with pipeline binding
        // This is a placeholder implementation
        return tekki::core::Result<void>::Ok();
    });
}

void SimpleRenderPass::Dispatch(const glm::u32vec2& extent) {
    Dispatch(glm::u32vec3(extent.x, extent.y, 1));
}

void SimpleRenderPass::TraceRays(
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
    const glm::u32vec3& extent
) {
    // For now, just call render with basic ray tracing
    pass_.Render([extent](RenderPassApi& api) {
        // TODO: Implement proper ray tracing with pipeline binding
        // This is a placeholder implementation
        return tekki::core::Result<void>::Ok();
    });
}

} // namespace tekki::render_graph