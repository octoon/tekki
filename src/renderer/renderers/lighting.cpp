#include "tekki/renderer/renderers/lighting.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/lib.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace tekki::renderer::renderers {

LightingRenderer::LightingRenderer() = default;

void LightingRenderer::RenderSpecular(
    rg::Handle<tekki::backend::vulkan::Image>& outputTex,
    rg::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    vk::DescriptorSet bindlessDescriptorSet,
    const rg::Handle<tekki::backend::vulkan::RayTracingAcceleration>& tlas
) {
    try {
        auto gbufferDesc = gbufferDepth.gbuffer.desc();

        auto refl0Tex = rg.create(
            gbufferDesc
                .usage(vk::ImageUsageFlags())
                .half_res()
                .format(vk::Format::R16G16B16A16_SFLOAT)
        );

        // When using PDFs stored wrt to the surface area metric, their values can be tiny or giant,
        // so fp32 is necessary. The projected solid angle metric is less sensitive, but that shader
        // variant is heavier. Overall the surface area metric and fp32 combo is faster on my RTX 2080.
        auto refl1Tex = rg.create(refl0Tex.desc().format(vk::Format::R32G32B32A32_SFLOAT));

        auto refl2Tex = rg.create(refl0Tex.desc().format(vk::Format::R8G8B8A8_SNORM));

        auto refl0TexMut = refl0Tex;
        auto refl1TexMut = refl1Tex;
        auto refl2TexMut = refl2Tex;

        rg::SimpleRenderPass::new_rt(
            rg.add_pass("sample lights"),
            tekki::backend::vulkan::ShaderSource::hlsl("/shaders/lighting/sample_lights.rgen.hlsl"),
            {
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {tekki::backend::vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
        )
        .read_aspect(&gbufferDepth.depth, vk::ImageAspectFlagBits::eDepth)
        .write(&refl0TexMut)
        .write(&refl1TexMut)
        .write(&refl2TexMut)
        .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .trace_rays(&tlas, refl0Tex.desc().extent);

        auto halfViewNormalTex = gbufferDepth.half_view_normal(rg);
        auto halfDepthTex = gbufferDepth.half_depth(rg);

        rg::SimpleRenderPass::new_compute(
            rg.add_pass("spatial reuse lights"),
            "/shaders/lighting/spatial_reuse_lights.hlsl"
        )
        .read(&gbufferDepth.gbuffer)
        .read_aspect(&gbufferDepth.depth, vk::ImageAspectFlagBits::eDepth)
        .read(&refl0Tex)
        .read(&refl1Tex)
        .read(&refl2Tex)
        .read(&*halfViewNormalTex)
        .read(&*halfDepthTex)
        .write(&outputTex)
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .constants(std::make_tuple(
            outputTex.desc().extent_inv_extent_2d(),
            SpatialResolveOffsets::OFFSETS
        ))
        .dispatch(outputTex.desc().extent);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("LightingRenderer::RenderSpecular failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers