#include "tekki/renderer/renderers/ssgi.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"
#include "tekki/render_graph/lib.h"
#include "tekki/render_graph/simple_render_pass.h"
#include <glm/glm.hpp>

namespace rg = tekki::render_graph;

namespace tekki::renderer::renderers {

SsgiRenderer::SsgiRenderer() : ssgiTex("ssgi") {}

tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> SsgiRenderer::Render(
    tekki::render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& prevRadiance,
    VkDescriptorSet bindlessDescriptorSet
) {
    auto gbufferDesc = gbufferDepth.gbuffer.desc;
    auto halfViewNormalTex = gbufferDepth.half_view_normal(*rg.GetRenderGraph());
    auto halfDepthTex = gbufferDepth.half_depth(*rg.GetRenderGraph());

    auto ssgiOutputTex = rg.Create(
        rg::ImageDesc::New2d(
            INTERNAL_TEX_FMT,
            glm::u32vec2(gbufferDesc.Extent.x / 2, gbufferDesc.Extent.y / 2)
        ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    if (USE_RUST_SHADERS) {
        auto pass = rg.AddPass("ssao");
        rg::SimpleRenderPass::NewComputeRust(pass, "ssgi::ssgi_cs")
            .Read(gbufferDepth.gbuffer)
            .Read(halfDepthTex)
            .Read(halfViewNormalTex)
            .Read(prevRadiance)
            .Read(reprojectionMap)
            .Write(ssgiOutputTex)
            .Constants(std::make_tuple(
                glm::vec4(gbufferDesc.Extent.x, gbufferDesc.Extent.y, 1.0f/gbufferDesc.Extent.x, 1.0f/gbufferDesc.Extent.y),
                glm::vec4(ssgiOutputTex.desc.Extent.x, ssgiOutputTex.desc.Extent.y,
                         1.0f/ssgiOutputTex.desc.Extent.x, 1.0f/ssgiOutputTex.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec3(ssgiOutputTex.desc.Extent.x, ssgiOutputTex.desc.Extent.y, 1));
    } else {
        auto pass = rg.AddPass("ssao");
        rg::SimpleRenderPass::NewCompute(pass, "/shaders/ssgi/ssgi.hlsl")
            .Read(gbufferDepth.gbuffer)
            .Read(halfDepthTex)
            .Read(halfViewNormalTex)
            .Read(prevRadiance)
            .Read(reprojectionMap)
            .Write(ssgiOutputTex)
            .Constants(std::make_tuple(
                glm::vec4(gbufferDesc.Extent.x, gbufferDesc.Extent.y, 1.0f/gbufferDesc.Extent.x, 1.0f/gbufferDesc.Extent.y),
                glm::vec4(ssgiOutputTex.desc.Extent.x, ssgiOutputTex.desc.Extent.y,
                         1.0f/ssgiOutputTex.desc.Extent.x, 1.0f/ssgiOutputTex.desc.Extent.y)
            ))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Dispatch(glm::u32vec3(ssgiOutputTex.desc.Extent.x, ssgiOutputTex.desc.Extent.y, 1));
    }

    return FilterSsgi(
        rg,
        ssgiOutputTex,
        gbufferDepth,
        reprojectionMap,
        ssgiTex
    );
}

tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> SsgiRenderer::FilterSsgi(
    tekki::render_graph::TemporalRenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input,
    const GbufferDepth& gbufferDepth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
    PingPongTemporalResource& temporalTex
) {
    auto gbufferDesc = gbufferDepth.gbuffer.desc;
    auto halfViewNormalTex = gbufferDepth.half_view_normal(*rg.GetRenderGraph());
    auto halfDepthTex = gbufferDepth.half_depth(*rg.GetRenderGraph());

    auto upsampledTex = [&]() {
        auto spatiallyFilteredTex = rg.Create(
            rg::ImageDesc::New2d(
                INTERNAL_TEX_FMT,
                glm::u32vec2(gbufferDesc.Extent.x / 2, gbufferDesc.Extent.y / 2)
            ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        if (USE_RUST_SHADERS) {
            auto pass = rg.AddPass("ssao spatial");
            auto renderPass = rg::SimpleRenderPass::NewComputeRust(pass, "ssgi::spatial_filter_cs");
            renderPass
                .Read(input)
                .Read(halfDepthTex)
                .Read(halfViewNormalTex)
                .Write(spatiallyFilteredTex)
                .Dispatch(glm::u32vec3(spatiallyFilteredTex.desc.Extent.x, spatiallyFilteredTex.desc.Extent.y, 1));
        } else {
            auto pass = rg.AddPass("ssao spatial");
            auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/ssgi/spatial_filter.hlsl");
            renderPass
                .Read(input)
                .Read(halfDepthTex)
                .Read(halfViewNormalTex)
                .Write(spatiallyFilteredTex)
                .Dispatch(glm::u32vec3(spatiallyFilteredTex.desc.Extent.x, spatiallyFilteredTex.desc.Extent.y, 1));
        }

        return UpsampleSsgi(
            *rg.GetRenderGraph(),
            spatiallyFilteredTex,
            gbufferDepth.depth,
            gbufferDepth.gbuffer
        );
    }();

    auto [historyOutputTex, historyTex] = temporalTex
        .GetOutputAndHistory(rg, TemporalTexDesc(glm::u32vec2(gbufferDesc.Extent.x, gbufferDesc.Extent.y)));

    auto filteredOutputTex = rg.Create(
        rg::ImageDesc::New2d(FINAL_TEX_FMT, glm::u32vec2(gbufferDesc.Extent.x, gbufferDesc.Extent.y))
    );

    if (USE_RUST_SHADERS) {
        auto pass = rg.AddPass("ssao temporal");
        auto renderPass = rg::SimpleRenderPass::NewComputeRust(pass, "ssgi::temporal_filter_cs");
        renderPass
            .Read(upsampledTex)
            .Read(historyTex)
            .Read(reprojectionMap)
            .Write(filteredOutputTex)
            .Write(historyOutputTex)
            .Constants(std::make_tuple(
                glm::vec4(historyOutputTex.desc.Extent.x, historyOutputTex.desc.Extent.y,
                         1.0f/historyOutputTex.desc.Extent.x, 1.0f/historyOutputTex.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec3(historyOutputTex.desc.Extent.x, historyOutputTex.desc.Extent.y, 1));
    } else {
        auto pass = rg.AddPass("ssao temporal");
        auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/ssgi/temporal_filter.hlsl");
        renderPass
            .Read(upsampledTex)
            .Read(historyTex)
            .Read(reprojectionMap)
            .Write(filteredOutputTex)
            .Write(historyOutputTex)
            .Constants(std::make_tuple(
                glm::vec4(historyOutputTex.desc.Extent.x, historyOutputTex.desc.Extent.y,
                         1.0f/historyOutputTex.desc.Extent.x, 1.0f/historyOutputTex.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec3(historyOutputTex.desc.Extent.x, historyOutputTex.desc.Extent.y, 1));
    }

    return rg::ReadOnlyHandle<tekki::backend::vulkan::Image>(filteredOutputTex);
}

tekki::render_graph::ImageDesc SsgiRenderer::TemporalTexDesc(const glm::u32vec2& extent) {
    return tekki::render_graph::ImageDesc::New2d(
        INTERNAL_TEX_FMT,
        extent
    ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

tekki::render_graph::Handle<tekki::backend::vulkan::Image> SsgiRenderer::UpsampleSsgi(
    tekki::render_graph::RenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& ssgi,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& depth,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& gbuffer
) {
    auto gbufferDesc = gbuffer.desc;
    auto outputTex = rg.Create(
        rg::ImageDesc::New2d(INTERNAL_TEX_FMT, glm::u32vec2(gbufferDesc.Extent.x, gbufferDesc.Extent.y))
    );

    if (USE_RUST_SHADERS) {
        auto pass = rg.AddPass("ssao upsample");
        auto renderPass = rg::SimpleRenderPass::NewComputeRust(pass, "ssgi::upsample_cs");
        renderPass
            .Read(ssgi)
            .ReadAspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(gbuffer)
            .Write(outputTex)
            .Dispatch(glm::u32vec3(outputTex.desc.Extent.x, outputTex.desc.Extent.y, 1));
    } else {
        auto pass = rg.AddPass("ssao upsample");
        auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/ssgi/upsample.hlsl");
        renderPass
            .Read(ssgi)
            .ReadAspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(gbuffer)
            .Write(outputTex)
            .Dispatch(glm::u32vec3(outputTex.desc.Extent.x, outputTex.desc.Extent.y, 1));
    }

    return outputTex;
}

} // namespace tekki::renderer::renderers