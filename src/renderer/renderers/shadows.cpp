#include "tekki/renderer/renderers/shadows.h"
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/render_graph.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "gbuffer_depth.h"

namespace tekki::renderer::renderers {

std::shared_ptr<tekki::backend::vulkan::Image> Shadows::TraceSunShadowMask(
    std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph,
    std::shared_ptr<GbufferDepth> gbufferDepth,
    std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration> tlas,
    VkDescriptorSet bindlessDescriptorSet
) {
    auto outputImage = renderGraph->Create(gbufferDepth->Depth->GetDesc().Format(VK_FORMAT_R8_UNORM));

    auto renderPass = std::make_shared<tekki::render_graph::SimpleRenderPass>(
        renderGraph->AddPass("trace shadow mask"),
        tekki::backend::vulkan::ShaderSource::Hlsl("/shaders/rt/trace_sun_shadow_mask.rgen.hlsl")
    );

    std::vector<tekki::backend::vulkan::ShaderSource> missShaders = {
        tekki::backend::vulkan::ShaderSource::Hlsl("/shaders/rt/shadow.rmiss.hlsl"),
        tekki::backend::vulkan::ShaderSource::Hlsl("/shaders/rt/shadow.rmiss.hlsl")
    };

    renderPass->SetRayTracingShaders(missShaders, std::vector<tekki::backend::vulkan::ShaderSource>());
    renderPass->ReadAspect(gbufferDepth->Depth, VK_IMAGE_ASPECT_DEPTH_BIT);
    renderPass->Read(gbufferDepth->GeometricNormal);
    renderPass->Write(outputImage);
    renderPass->SetRawDescriptorSet(1, bindlessDescriptorSet);
    renderPass->TraceRays(tlas, outputImage->GetDesc().Extent);

    return outputImage;
}

} // namespace tekki::renderer::renderers