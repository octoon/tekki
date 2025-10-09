#include "tekki/renderer/renderers/ssgi.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"
#include "tekki/render_graph/simple_render_pass.h"
#include <glm/glm.hpp>

namespace tekki::renderer::renderers {

SsgiRenderer::SsgiRenderer() : ssgi_tex("ssgi") {}

tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> SsgiRenderer::Render(
    tekki::render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& prevRadiance,
    VkDescriptorSet bindlessDescriptorSet
) {
    auto gbufferDesc = gbufferDepth.gbuffer->Desc();
    auto halfViewNormalTex = gbufferDepth.HalfViewNormal(rg);
    auto halfDepthTex = gbufferDepth.HalfDepth(rg);

    auto ssgiTex = rg.Create(
        gbufferDesc
            .Usage(VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM)
            .HalfRes()
            .Format(INTERNAL_TEX_FMT)
    );

    if (USE_RUST_SHADERS) {
        SimpleRenderPass::NewComputeRust(rg.AddPass("ssao"), "ssgi::ssgi_cs")
            .Read(gbufferDepth.gbuffer)
            .Read(halfDepthTex)
            .Read(halfViewNormalTex)
            .Read(prevRadiance)
            .Read(reprojectionMap)
            .Write(ssgiTex)
            .Constants(SsgiConstants::DefaultWithSize(
                gbufferDesc.ExtentInvExtent2D(),
                ssgiTex->Desc().ExtentInvExtent2D()
            ))
            .Dispatch(ssgiTex->Desc().Extent);
    } else {
        SimpleRenderPass::NewCompute(rg.AddPass("ssao"), "/shaders/ssgi/ssgi.hlsl")
            .Read(gbufferDepth.gbuffer)
            .Read(halfDepthTex)
            .Read(halfViewNormalTex)
            .Read(prevRadiance)
            .Read(reprojectionMap)
            .Write(ssgiTex)
            .Constants(std::make_tuple(
                gbufferDesc.ExtentInvExtent2D(),
                ssgiTex->Desc().ExtentInvExtent2D()
            ))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Dispatch(ssgiTex->Desc().Extent);
    }

    return FilterSsgi(
        rg,
        ssgiTex,
        gbufferDepth,
        reprojectionMap,
        ssgi_tex
    );
}

tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> SsgiRenderer::FilterSsgi(
    tekki::render_graph::TemporalRenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input,
    const GbufferDepth& gbufferDepth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
    PingPongTemporalResource& temporalTex
) {
    auto gbufferDesc = gbufferDepth.gbuffer->Desc();
    auto halfViewNormalTex = gbufferDepth.HalfViewNormal(rg);
    auto halfDepthTex = gbufferDepth.HalfDepth(rg);

    auto upsampledTex = [&]() {
        auto spatiallyFilteredTex = rg.Create(
            gbufferDesc
                .Usage(VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM)
                .HalfRes()
                .Format(INTERNAL_TEX_FMT)
        );

        if (USE_RUST_SHADERS) {
            SimpleRenderPass::NewComputeRust(
                rg.AddPass("ssao spatial"),
                "ssgi::spatial_filter_cs"
            )
        } else {
            SimpleRenderPass::NewCompute(
                rg.AddPass("ssao spatial"),
                "/shaders/ssgi/spatial_filter.hlsl"
            )
        }
        .Read(input)
        .Read(halfDepthTex)
        .Read(halfViewNormalTex)
        .Write(spatiallyFilteredTex)
        .Dispatch(spatiallyFilteredTex->Desc().Extent);

        return UpsampleSsgi(
            rg,
            spatiallyFilteredTex,
            gbufferDepth.depth,
            gbufferDepth.gbuffer
        );
    }();

    auto [historyOutputTex, historyTex] = temporalTex
        .GetOutputAndHistory(rg, TemporalTexDesc(gbufferDesc.Extent2D()));

    auto filteredOutputTex = rg.Create(gbufferDesc.Format(FINAL_TEX_FMT));

    if (USE_RUST_SHADERS) {
        SimpleRenderPass::NewComputeRust(
            rg.AddPass("ssao temporal"),
            "ssgi::temporal_filter_cs"
        )
    } else {
        SimpleRenderPass::NewCompute(
            rg.AddPass("ssao temporal"),
            "/shaders/ssgi/temporal_filter.hlsl"
        )
    }
    .Read(upsampledTex)
    .Read(historyTex)
    .Read(reprojectionMap)
    .Write(filteredOutputTex)
    .Write(historyOutputTex)
    .Constants(historyOutputTex->Desc().ExtentInvExtent2D())
    .Dispatch(historyOutputTex->Desc().Extent);

    return filteredOutputTex;
}

tekki::backend::vulkan::ImageDesc SsgiRenderer::TemporalTexDesc(const glm::u32vec2& extent) {
    return tekki::backend::vulkan::ImageDesc::New2D(
        INTERNAL_TEX_FMT, 
        glm::u32vec2(extent.x, extent.y)
    ).Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

tekki::render_graph::Handle<tekki::backend::vulkan::Image> SsgiRenderer::UpsampleSsgi(
    tekki::render_graph::RenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& ssgi,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& depth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& gbuffer
) {
    auto outputTex = rg.Create(gbuffer->Desc().Format(INTERNAL_TEX_FMT));

    if (USE_RUST_SHADERS) {
        SimpleRenderPass::NewComputeRust(rg.AddPass("ssao upsample"), "ssgi::upsample_cs")
    } else {
        SimpleRenderPass::NewCompute(
            rg.AddPass("ssao upsample"),
            "/shaders/ssgi/upsample.hlsl"
        )
    }
    .Read(ssgi)
    .ReadAspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT)
    .Read(gbuffer)
    .Write(outputTex)
    .Dispatch(outputTex->Desc().Extent);
    
    return outputTex;
}

} // namespace tekki::renderer::renderers