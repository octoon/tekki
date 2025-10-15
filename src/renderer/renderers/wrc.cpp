#include "tekki/renderer/renderers/wrc.h"
#include "tekki/renderer/renderers/ircache_render_state.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "tekki/render_graph/Image.h"
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
        // Create ray tracing pass using SimpleRenderPass
        auto passBuilder = rg.AddPass("wrc see through");
        auto pass = tekki::render_graph::SimpleRenderPass::NewRayTracing(
            passBuilder,
            tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/wrc/wrc_see_through.rgen.hlsl"),
            {
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")
            }
        );

        // Bind resources
        auto& outputDesc = outputImg.Desc();
        pass.Read(this->RadianceAtlas)
            .Read(skyCube)
            .Write(outputImg)
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .TraceRays(tlas, glm::u32vec3(outputDesc.Extent.x, outputDesc.Extent.y, outputDesc.Extent.z));

        // Bind ircache via trait if needed
        (void)ircache; // Will be bound via trait system when implemented

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

        auto radianceAtlas = rg.Create(
            tekki::backend::vulkan::ImageDesc::New2d(
                VK_FORMAT_R32G32B32A32_SFLOAT,
                glm::u32vec2(
                    WrcAtlasProbeCount[0] * WrcProbeDims,
                    WrcAtlasProbeCount[1] * WrcProbeDims
                )
            )
        );

        // Create ray tracing pass using SimpleRenderPass
        auto passBuilder = rg.AddPass("wrc trace");
        auto pass = tekki::render_graph::SimpleRenderPass::NewRayTracing(
            passBuilder,
            tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/wrc/trace_wrc.rgen.hlsl"),
            {
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {
                tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")
            }
        );

        // Bind resources
        pass.Read(skyCube)
            .Write(radianceAtlas)
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .TraceRays(tlas, glm::u32vec3(totalPixelCount, 1, 1));

        // Bind ircache via trait if needed
        (void)ircache; // Will be bound via trait system when implemented

        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::WrcTrace failed: ") + e.what());
    }
}

WrcRenderState WrcRenderer::AllocateDummyOutput(tekki::render_graph::TemporalRenderGraph& rg) {
    try {
        auto radianceAtlas = rg.Create(
            tekki::backend::vulkan::ImageDesc::New2d(
                VK_FORMAT_R8_UNORM,
                glm::u32vec2(1, 1)
            )
        );
        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::AllocateDummyOutput failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers