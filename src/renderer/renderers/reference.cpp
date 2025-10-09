#include "tekki/renderer/renderers/reference.h"
#include <stdexcept>
#include <string>

namespace tekki::renderer::renderers {

void ReferencePathTrace::Execute(
    RenderGraph& renderGraph,
    std::shared_ptr<Image>& outputImage,
    VkDescriptorSet bindlessDescriptorSet,
    std::shared_ptr<RayTracingAcceleration> tlas
) {
    try {
        auto pass = renderGraph.AddPass("reference pt");
        
        SimpleRenderPass renderPass(pass);
        renderPass.SetRayTracingShader(
            ShaderSource::FromHlsl("/shaders/rt/reference_path_trace.rgen.hlsl")
        );
        
        std::vector<ShaderSource> missShaders = {
            ShaderSource::FromHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
            ShaderSource::FromHlsl("/shaders/rt/shadow.rmiss.hlsl")
        };
        renderPass.SetMissShaders(missShaders);
        
        std::vector<ShaderSource> hitShaders = {
            ShaderSource::FromHlsl("/shaders/rt/gbuffer.rchit.hlsl")
        };
        renderPass.SetHitShaders(hitShaders);
        
        renderPass.Write(outputImage);
        renderPass.SetRawDescriptorSet(1, bindlessDescriptorSet);
        
        auto extent = outputImage->GetDescription().extent;
        renderPass.TraceRays(tlas, extent);
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Reference path trace failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers