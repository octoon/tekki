#include "tekki/renderer/renderers/wrc.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vk_sync.h"
#include <stdexcept>
#include <memory>

namespace tekki::renderer::renderers {

void WrcRenderState::SeeThrough(
    tekki::render_graph::TemporalRenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& skyCube,
    IrcacheRenderState& ircache,
    VkDescriptorSet bindlessDescriptorSet,
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
    tekki::render_graph::Handle<tekki::backend::vulkan::Image>& outputImg
) {
    try {
        tekki::render_graph::PassBuilder pass = rg.AddPass("wrc see through");

        // Create ray tracing pipeline with shaders
        std::vector<tekki::render_graph::PipelineShaderDesc> shaders;
        shaders.push_back({
            "/shaders/wrc/wrc_see_through.rgen.hlsl",
            "main",
            VK_SHADER_STAGE_RAYGEN_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/gbuffer.rmiss.hlsl",
            "main",
            VK_SHADER_STAGE_MISS_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/shadow.rmiss.hlsl",
            "main",
            VK_SHADER_STAGE_MISS_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/gbuffer.rchit.hlsl",
            "main",
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
        });

        tekki::render_graph::RayTracingPipelineDesc rtDesc{};
        auto pipeline = pass.RegisterRayTracingPipeline(shaders, rtDesc);

        auto radiance_ref = pass.Read(RadianceAtlas, vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
        auto sky_ref = pass.Read(skyCube, vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
        auto output_ref = pass.Write(outputImg, vk_sync::AccessType::ComputeShaderWrite);

        pass.Render([pipeline, radiance_ref, sky_ref, output_ref, bindlessDescriptorSet, tlas](tekki::render_graph::RenderPassApi& api) {
            // TODO: Implement ray tracing dispatch when RenderPassApi is complete
            (void)pipeline;
            (void)radiance_ref;
            (void)sky_ref;
            (void)output_ref;
            (void)bindlessDescriptorSet;
            (void)tlas;
        });
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderState::SeeThrough failed: ") + e.what());
    }
}

WrcRenderState WrcRenderer::WrcTrace(
    tekki::render_graph::TemporalRenderGraph& rg,
    IrcacheRenderState& ircache,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas
) {
    try {
        uint32_t totalProbeCount = WrcGridDims[0] * WrcGridDims[1] * WrcGridDims[2];
        uint32_t totalPixelCount = totalProbeCount * WrcProbeDims * WrcProbeDims;

        auto desc = tekki::backend::vulkan::ImageDesc::New2d(
            VK_FORMAT_R32G32B32A32_SFLOAT,
            {
                WrcAtlasProbeCount[0] * WrcProbeDims,
                WrcAtlasProbeCount[1] * WrcProbeDims
            }
        );

        auto radianceAtlas = rg.Create(desc);

        tekki::render_graph::PassBuilder pass = rg.AddPass("wrc trace");

        // Create ray tracing pipeline with shaders
        std::vector<tekki::render_graph::PipelineShaderDesc> shaders;
        shaders.push_back({
            "/shaders/wrc/trace_wrc.rgen.hlsl",
            "main",
            VK_SHADER_STAGE_RAYGEN_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/gbuffer.rmiss.hlsl",
            "main",
            VK_SHADER_STAGE_MISS_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/shadow.rmiss.hlsl",
            "main",
            VK_SHADER_STAGE_MISS_BIT_KHR
        });
        shaders.push_back({
            "/shaders/rt/gbuffer.rchit.hlsl",
            "main",
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
        });

        tekki::render_graph::RayTracingPipelineDesc rtDesc{};
        auto pipeline = pass.RegisterRayTracingPipeline(shaders, rtDesc);

        auto sky_ref = pass.Read(skyCube, vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
        auto radiance_ref = pass.Write(radianceAtlas, vk_sync::AccessType::ComputeShaderWrite);

        pass.Render([pipeline, sky_ref, radiance_ref, bindlessDescriptorSet, tlas, totalPixelCount](tekki::render_graph::RenderPassApi& api) {
            // TODO: Implement ray tracing dispatch when RenderPassApi is complete
            (void)pipeline;
            (void)sky_ref;
            (void)radiance_ref;
            (void)bindlessDescriptorSet;
            (void)tlas;
            (void)totalPixelCount;
        });

        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::WrcTrace failed: ") + e.what());
    }
}

WrcRenderState WrcRenderer::AllocateDummyOutput(tekki::render_graph::TemporalRenderGraph& rg) {
    try {
        auto desc = tekki::backend::vulkan::ImageDesc::New2d(
            VK_FORMAT_R8_UNORM,
            {1, 1}
        );
        auto radianceAtlas = rg.Create(desc);
        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::AllocateDummyOutput failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers