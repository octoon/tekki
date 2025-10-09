#include "tekki/renderer/renderers/wrc.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include <stdexcept>
#include <memory>

namespace tekki::renderer::renderers {

void WrcRenderState::SeeThrough(
    tekki::render_graph::TemporalRenderGraph& rg,
    const std::shared_ptr<tekki::render_graph::Image>& skyCube,
    IrcacheRenderState& ircache,
    VkDescriptorSet bindlessDescriptorSet,
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
    std::shared_ptr<tekki::render_graph::Image>& outputImg
) {
    try {
        auto pass = rg.AddPass("wrc see through");
        
        tekki::render_graph::SimpleRenderPass::new_rt(
            pass,
            tekki::backend::vulkan::ShaderSource::hlsl("/shaders/wrc/wrc_see_through.rgen.hlsl"),
            {
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
        )
        .read(RadianceAtlas)
        .read(skyCube)
        .bind_mut(ircache)
        .write(outputImg)
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .trace_rays(tlas, outputImg->desc().extent);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderState::SeeThrough failed: ") + e.what());
    }
}

WrcRenderState WrcRenderer::WrcTrace(
    tekki::render_graph::TemporalRenderGraph& rg,
    IrcacheRenderState& ircache,
    const std::shared_ptr<tekki::render_graph::Image>& skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas
) {
    try {
        uint32_t totalProbeCount = WrcGridDims[0] * WrcGridDims[1] * WrcGridDims[2];
        uint32_t totalPixelCount = totalProbeCount * WrcProbeDims * WrcProbeDims;

        auto radianceAtlas = rg.create(tekki::backend::vulkan::ImageDesc::new_2d(
            VK_FORMAT_R32G32B32A32_SFLOAT,
            {
                WrcAtlasProbeCount[0] * WrcProbeDims,
                WrcAtlasProbeCount[1] * WrcProbeDims
            }
        ));

        auto pass = rg.AddPass("wrc trace");
        
        tekki::render_graph::SimpleRenderPass::new_rt(
            pass,
            tekki::backend::vulkan::ShaderSource::hlsl("/shaders/wrc/trace_wrc.rgen.hlsl"),
            {
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
        )
        .read(skyCube)
        .bind_mut(ircache)
        .write(radianceAtlas)
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .trace_rays(tlas, {totalPixelCount, 1, 1});

        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::WrcTrace failed: ") + e.what());
    }
}

WrcRenderState WrcRenderer::AllocateDummyOutput(tekki::render_graph::TemporalRenderGraph& rg) {
    try {
        auto radianceAtlas = rg.create(tekki::backend::vulkan::ImageDesc::new_2d(
            VK_FORMAT_R8_UNORM,
            {1, 1}
        ));
        return WrcRenderState(radianceAtlas);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WrcRenderer::AllocateDummyOutput failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers