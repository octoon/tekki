#include "tekki/renderer/renderers/rtdgi.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "tekki/backend/vulkan/shader.h"
#include <glm/glm.hpp>
#include <memory>
#include <stdexcept>

namespace rg = tekki::render_graph;
using ShaderSource = tekki::backend::vulkan::ShaderSource;

namespace tekki::renderer::renderers {

RtdgiRenderer::RtdgiRenderer() :
    TemporalRadianceTex("rtdgi.radiance"),
    TemporalRayOrigTex("rtdgi.ray_orig"),
    TemporalRayTex("rtdgi.ray"),
    TemporalReservoirTex("rtdgi.reservoir"),
    TemporalCandidateTex("rtdgi.candidate"),
    TemporalInvalidityTex("rtdgi.invalidity"),
    Temporal2Tex("rtdgi.temporal2"),
    Temporal2VarianceTex("rtdgi.temporal2_var"),
    TemporalHitNormalTex("rtdgi.hit_normal"),
    SpatialReusePassCount(2),
    UseRaytracedReservoirVisibility(false)
{
}

rg::ImageDesc RtdgiRenderer::TemporalTexDesc(const std::array<uint32_t, 2>& extent) {
    return rg::ImageDesc::New2d(COLOR_BUFFER_FORMAT, glm::u32vec2(extent[0], extent[1]))
        .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

rg::Handle<Image> RtdgiRenderer::Temporal(
    TemporalRenderGraph& rg,
    const rg::Handle<Image>& inputColor,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& reprojectionMap,
    const rg::Handle<Image>& reprojectedHistoryTex,
    const rg::Handle<Image>& rtHistoryInvalidityTex,
    rg::Handle<Image> temporalOutputTex)
{
    try {
        auto [temporalVarianceOutputTex, varianceHistoryTex] = Temporal2VarianceTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(VK_FORMAT_R16G16_SFLOAT, glm::u32vec2(inputColor.desc.Extent.x, inputColor.desc.Extent.y))
                .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto temporalFilteredTex = rg.Create(
            rg::ImageDesc(gbufferDepth.gbuffer.desc)
                .WithUsage(VkImageUsageFlags(0))
                .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        auto pass = rg.AddPass("rtdgi temporal");
        auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/rtdgi/temporal_filter.hlsl");
        renderPass
            .Read(inputColor)
            .Read(reprojectedHistoryTex)
            .Read(varianceHistoryTex)
            .Read(reprojectionMap)
            .Read(rtHistoryInvalidityTex)
            .Write(temporalFilteredTex)
            .Write(temporalOutputTex)
            .Write(temporalVarianceOutputTex)
            .Constants(std::make_tuple(
                temporalOutputTex.desc.GetExtentInvExtent2d(),
                gbufferDepth.gbuffer.desc.GetExtentInvExtent2d()
            ))
            .Dispatch(temporalOutputTex.desc.Extent);

        return temporalFilteredTex;
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Temporal failed: " + std::string(e.what()));
    }
}

rg::Handle<Image> RtdgiRenderer::Spatial(
    TemporalRenderGraph& rg,
    const rg::Handle<Image>& inputColor,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& ssaoTex,
    VkDescriptorSet bindlessDescriptorSet)
{
    try {
        auto spatialFilteredTex = rg.Create(TemporalTexDesc({inputColor.desc.Extent.x, inputColor.desc.Extent.y}));

        auto pass = rg.AddPass("rtdgi spatial");
        auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/rtdgi/spatial_filter.hlsl");
        renderPass
            .Read(inputColor)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(ssaoTex)
            .Read(gbufferDepth.geometric_normal)
            .Write(spatialFilteredTex)
            .Constants(std::make_tuple(spatialFilteredTex.desc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Dispatch(spatialFilteredTex.desc.Extent);

        return spatialFilteredTex;
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Spatial failed: " + std::string(e.what()));
    }
}

ReprojectedRtdgi RtdgiRenderer::Reproject(
    TemporalRenderGraph& rg,
    const rg::Handle<Image>& reprojectionMap)
{
    try {
        auto gbufferExtent = std::array<uint32_t, 2>{reprojectionMap.desc.Extent.x, reprojectionMap.desc.Extent.y};

        auto [temporalOutputTex, historyTex] = Temporal2Tex.GetOutputAndHistory(
            rg, TemporalTexDesc(gbufferExtent)
        );

        auto reprojectedHistoryTex = rg.Create(TemporalTexDesc(gbufferExtent));

        auto pass = rg.AddPass("rtdgi reproject");
        auto renderPass = rg::SimpleRenderPass::NewCompute(pass, "/shaders/rtdgi/fullres_reproject.hlsl");
        renderPass
            .Read(historyTex)
            .Read(reprojectionMap)
            .Write(reprojectedHistoryTex)
            .Constants(std::make_tuple(reprojectedHistoryTex.desc.GetExtentInvExtent2d()))
            .Dispatch(reprojectedHistoryTex.desc.Extent);

        return ReprojectedRtdgi{
            reprojectedHistoryTex,
            temporalOutputTex
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Reproject failed: " + std::string(e.what()));
    }
}

RtdgiOutput RtdgiRenderer::Render(
    TemporalRenderGraph& rg,
    const ReprojectedRtdgi& reprojectedRtdgi,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& reprojectionMap,
    const rg::Handle<Image>& skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    const rg::Handle<RayTracingAcceleration>& tlas,
    const rg::Handle<Image>& ssaoTex)
{
    try {
        auto halfSsaoTex = rg.Create(
            rg::ImageDesc(ssaoTex.desc)
                .WithHalfRes()
                .WithUsage(VkImageUsageFlags(0))
                .WithFormat(VK_FORMAT_R8_SNORM)
        );

        auto pass1 = rg.AddPass("extract ssao/2");
        auto renderPass1 = rg::SimpleRenderPass::NewCompute(pass1, "/shaders/extract_half_res_ssao.hlsl");
        renderPass1
            .Read(ssaoTex)
            .Write(halfSsaoTex)
            .Dispatch(halfSsaoTex.desc.Extent);

        auto gbufferDesc = gbufferDepth.gbuffer.desc;

        auto [hitNormalOutputTex, hitNormalHistoryTex] = TemporalHitNormalTex.GetOutputAndHistory(
            rg,
            TemporalTexDesc({
                gbufferDesc
                    .WithFormat(VK_FORMAT_R8G8B8A8_UNORM)
                    .WithHalfRes()
                    .Extent.x,
                gbufferDesc
                    .WithFormat(VK_FORMAT_R8G8B8A8_UNORM)
                    .WithHalfRes()
                    .Extent.y
            })
        );

        auto [candidateOutputTex, candidateHistoryTex] = TemporalCandidateTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(
                VK_FORMAT_R16G16B16A16_SFLOAT,
                glm::u32vec2(gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y)
            )
            .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto candidateRadianceTex = rg.Create(
            gbufferDesc
                .WithHalfRes()
                .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        auto candidateNormalTex = rg.Create(
            gbufferDesc.WithHalfRes().WithFormat(VK_FORMAT_R8G8B8A8_SNORM)
        );

        auto candidateHitTex = rg.Create(
            gbufferDesc
                .WithHalfRes()
                .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        auto temporalReservoirPackedTex = rg.Create(
            gbufferDesc
                .WithHalfRes()
                .WithFormat(VK_FORMAT_R32G32B32A32_UINT)
        );

        auto halfDepthTex = gbufferDepth.half_depth(*rg.GetRenderGraph());

        auto [invalidityOutputTex, invalidityHistoryTex] = TemporalInvalidityTex.GetOutputAndHistory(
            rg,
            TemporalTexDesc({gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y})
                .WithFormat(VK_FORMAT_R16G16_SFLOAT)
        );

        auto [radianceTex, temporalReservoirTex] = [&]() -> std::tuple<rg::Handle<Image>, rg::Handle<Image>> {
            auto [radianceOutputTex, radianceHistoryTex] = TemporalRadianceTex.GetOutputAndHistory(
                rg,
                TemporalTexDesc({gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y})
            );

            auto [rayOrigOutputTex, rayOrigHistoryTex] = TemporalRayOrigTex.GetOutputAndHistory(
                rg,
                rg::ImageDesc::New2d(
                    VK_FORMAT_R32G32B32A32_SFLOAT,
                    glm::u32vec2(gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y)
                )
                .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            auto [rayOutputTex, rayHistoryTex] = TemporalRayTex.GetOutputAndHistory(
                rg,
                TemporalTexDesc({gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y})
                    .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            auto halfViewNormalTex = gbufferDepth.half_view_normal(*rg.GetRenderGraph());

            auto rtHistoryValidityPreInputTex = rg.Create(
                gbufferDesc.WithHalfRes().WithFormat(VK_FORMAT_R8_UNORM)
            );

            auto [reservoirOutputTex, reservoirHistoryTex] = TemporalReservoirTex.GetOutputAndHistory(
                rg,
                rg::ImageDesc::New2d(VK_FORMAT_R32G32_UINT, glm::u32vec2(gbufferDesc.WithHalfRes().Extent.x, gbufferDesc.WithHalfRes().Extent.y))
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            auto pass2 = rg.AddPass("rtdgi validate");
            rg::SimpleRenderPass::NewRt(
                pass2,
                ShaderSource::CreateHlsl("/shaders/rtdgi/diffuse_validate.rgen.hlsl"),
                {
                    ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .Read(halfViewNormalTex)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(reprojectedRtdgi.ReprojectedHistoryTex)
            .Write(reservoirHistoryTex)
            .Read(rayHistoryTex)
            .Read(reprojectionMap)
            .BindMut(ircache)
            .Bind(wrc)
            .Read(skyCube)
            .Write(radianceHistoryTex)
            .Read(rayOrigHistoryTex)
            .Write(rtHistoryValidityPreInputTex)
            .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .TraceRays(tlas, glm::u32vec3(candidateRadianceTex.desc.Extent.x, candidateRadianceTex.desc.Extent.y, candidateRadianceTex.desc.Extent.z));

            auto rtHistoryValidityInputTex = rg.Create(
                gbufferDesc.WithHalfRes().WithFormat(VK_FORMAT_R8_UNORM)
            );

            auto pass3 = rg.AddPass("rtdgi trace");
            rg::SimpleRenderPass::NewRt(
                pass3,
                ShaderSource::CreateHlsl("/shaders/rtdgi/trace_diffuse.rgen.hlsl"),
                {
                    ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .Read(halfViewNormalTex)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(reprojectedRtdgi.ReprojectedHistoryTex)
            .Read(reprojectionMap)
            .BindMut(ircache)
            .Bind(wrc)
            .Read(skyCube)
            .Read(rayOrigHistoryTex)
            .Write(candidateRadianceTex)
            .Write(candidateNormalTex)
            .Write(candidateHitTex)
            .Read(rtHistoryValidityPreInputTex)
            .Write(rtHistoryValidityInputTex)
            .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .TraceRays(tlas, glm::u32vec3(candidateRadianceTex.desc.Extent.x, candidateRadianceTex.desc.Extent.y, candidateRadianceTex.desc.Extent.z));

            auto pass4 = rg.AddPass("validity integrate");
            rg::SimpleRenderPass::NewCompute(
                pass4,
                "/shaders/rtdgi/temporal_validity_integrate.hlsl"
            )
            .Read(rtHistoryValidityInputTex)
            .Read(invalidityHistoryTex)
            .Read(reprojectionMap)
            .Read(halfViewNormalTex)
            .Read(halfDepthTex)
            .Write(invalidityOutputTex)
            .Constants(std::make_tuple(
                gbufferDesc.GetExtentInvExtent2d(),
                invalidityOutputTex.desc.GetExtentInvExtent2d()
            ))
            .Dispatch(invalidityOutputTex.desc.Extent);

            auto pass5 = rg.AddPass("restir temporal");
            rg::SimpleRenderPass::NewCompute(
                pass5,
                "/shaders/rtdgi/restir_temporal.hlsl"
            )
            .Read(halfViewNormalTex)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(candidateRadianceTex)
            .Read(candidateNormalTex)
            .Read(candidateHitTex)
            .Read(radianceHistoryTex)
            .Read(rayOrigHistoryTex)
            .Read(rayHistoryTex)
            .Read(reservoirHistoryTex)
            .Read(reprojectionMap)
            .Read(hitNormalHistoryTex)
            .Read(candidateHistoryTex)
            .Read(invalidityOutputTex)
            .Write(radianceOutputTex)
            .Write(rayOrigOutputTex)
            .Write(rayOutputTex)
            .Write(hitNormalOutputTex)
            .Write(reservoirOutputTex)
            .Write(candidateOutputTex)
            .Write(temporalReservoirPackedTex)
            .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Dispatch(radianceOutputTex.desc.Extent);

            return std::make_tuple(radianceOutputTex, reservoirOutputTex);
        }();

        auto irradianceTex = [&]() -> rg::Handle<Image> {
            auto halfViewNormalTex = gbufferDepth.half_view_normal(*rg.GetRenderGraph());

            auto reservoirOutputTex0 = rg.Create(
                gbufferDesc
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .WithHalfRes()
                    .WithFormat(VK_FORMAT_R32G32_UINT)
            );
            auto reservoirOutputTex1 = rg.Create(
                gbufferDesc
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .WithHalfRes()
                    .WithFormat(VK_FORMAT_R32G32_UINT)
            );

            auto bouncedRadianceOutputTex0 = rg.Create(
                gbufferDesc
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .WithHalfRes()
                    .WithFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            );
            auto bouncedRadianceOutputTex1 = rg.Create(
                gbufferDesc
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .WithHalfRes()
                    .WithFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            );

            auto reservoirInputTex = temporalReservoirTex;
            auto bouncedRadianceInputTex = radianceTex;

            for (uint32_t spatialReusePassIdx = 0; spatialReusePassIdx < SpatialReusePassCount; ++spatialReusePassIdx) {
                uint32_t performOcclusionRaymarch = (spatialReusePassIdx + 1 == SpatialReusePassCount) ? 1 : 0;
                uint32_t occlusionRaymarchImportanceOnly = UseRaytracedReservoirVisibility ? 1 : 0;

                auto pass6 = rg.AddPass("restir spatial");
                rg::SimpleRenderPass::NewCompute(
                    pass6,
                    "/shaders/rtdgi/restir_spatial.hlsl"
                )
                .Read(reservoirInputTex)
                .Read(bouncedRadianceInputTex)
                .Read(halfViewNormalTex)
                .Read(halfDepthTex)
                .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
                .Read(halfSsaoTex)
                .Read(temporalReservoirPackedTex)
                .Read(reprojectedRtdgi.ReprojectedHistoryTex)
                .Write(reservoirOutputTex0)
                .Write(bouncedRadianceOutputTex0)
                .Constants(std::make_tuple(
                    gbufferDesc.GetExtentInvExtent2d(),
                    reservoirOutputTex0.desc.GetExtentInvExtent2d(),
                    spatialReusePassIdx,
                    performOcclusionRaymarch,
                    occlusionRaymarchImportanceOnly
                ))
                .Dispatch(reservoirOutputTex0.desc.Extent);

                std::swap(reservoirOutputTex0, reservoirOutputTex1);
                std::swap(bouncedRadianceOutputTex0, bouncedRadianceOutputTex1);

                reservoirInputTex = reservoirOutputTex1;
                bouncedRadianceInputTex = bouncedRadianceOutputTex1;
            }

            if (UseRaytracedReservoirVisibility) {
                auto pass7 = rg.AddPass("restir check");
                rg::SimpleRenderPass::NewRt(
                    pass7,
                    ShaderSource::CreateHlsl("/shaders/rtdgi/restir_check.rgen.hlsl"),
                    {
                        ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                        ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
                    },
                    {ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")}
                )
                .Read(halfDepthTex)
                .Read(temporalReservoirPackedTex)
                .Write(reservoirInputTex)
                .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
                .RawDescriptorSet(1, bindlessDescriptorSet)
                .TraceRays(tlas, glm::u32vec3(candidateRadianceTex.desc.Extent.x, candidateRadianceTex.desc.Extent.y, candidateRadianceTex.desc.Extent.z));
            }

            auto irradianceOutputTex = rg.Create(
                gbufferDesc
                    .WithUsage(VkImageUsageFlags(0))
                    .WithFormat(COLOR_BUFFER_FORMAT)
            );

            auto pass8 = rg.AddPass("restir resolve");
            rg::SimpleRenderPass::NewCompute(
                pass8,
                "/shaders/rtdgi/restir_resolve.hlsl"
            )
            .Read(radianceTex)
            .Read(reservoirInputTex)
            .Read(gbufferDepth.gbuffer)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(halfViewNormalTex)
            .Read(halfDepthTex)
            .Read(ssaoTex)
            .Read(candidateRadianceTex)
            .Read(candidateHitTex)
            .Read(temporalReservoirPackedTex)
            .Read(bouncedRadianceInputTex)
            .Write(irradianceOutputTex)
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Constants(std::make_tuple(
                gbufferDesc.GetExtentInvExtent2d(),
                irradianceOutputTex.desc.GetExtentInvExtent2d()
            ))
            .Dispatch(irradianceOutputTex.desc.Extent);

            return irradianceOutputTex;
        }();

        auto filteredTex = Temporal(
            rg,
            irradianceTex,
            gbufferDepth,
            reprojectionMap,
            reprojectedRtdgi.ReprojectedHistoryTex,
            invalidityOutputTex,
            reprojectedRtdgi.TemporalOutputTex
        );

        filteredTex = Spatial(
            rg,
            filteredTex,
            gbufferDepth,
            ssaoTex,
            bindlessDescriptorSet
        );

        return RtdgiOutput{
            filteredTex,
            RtdgiCandidates{
                candidateRadianceTex,
                candidateNormalTex,
                candidateHitTex
            }
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Render failed: " + std::string(e.what()));
    }
}

}
