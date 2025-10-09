#include "tekki/renderer/renderers/rtdgi.h"
#include <glm/glm.hpp>
#include <memory>
#include <stdexcept>

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

ImageDesc RtdgiRenderer::TemporalTexDesc(const glm::uvec2& extent) {
    return ImageDesc::new_2d(COLOR_BUFFER_FORMAT, extent)
        .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

std::shared_ptr<Image> RtdgiRenderer::Temporal(
    TemporalRenderGraph& rg,
    const std::shared_ptr<Image>& inputColor,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<Image>& reprojectionMap,
    const std::shared_ptr<Image>& reprojectedHistoryTex,
    const std::shared_ptr<Image>& rtHistoryInvalidityTex,
    std::shared_ptr<Image> temporalOutputTex)
{
    try {
        auto [temporalVarianceOutputTex, varianceHistoryTex] = Temporal2VarianceTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(VK_FORMAT_R16G16_SFLOAT, inputColor->desc().extent_2d())
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto temporalFilteredTex = rg.create(
            gbufferDepth.gbuffer->desc()
                .usage(VkImageUsageFlags(0))
                .format(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        SimpleRenderPass::new_compute(
            rg.add_pass("rtdgi temporal"),
            "/shaders/rtdgi/temporal_filter.hlsl"
        )
        .read(inputColor)
        .read(reprojectedHistoryTex)
        .read(varianceHistoryTex)
        .read(reprojectionMap)
        .read(rtHistoryInvalidityTex)
        .write(temporalFilteredTex)
        .write(temporalOutputTex)
        .write(temporalVarianceOutputTex)
        .constants(std::make_tuple(
            temporalOutputTex->desc().extent_inv_extent_2d(),
            gbufferDepth.gbuffer->desc().extent_inv_extent_2d()
        ))
        .dispatch(temporalOutputTex->desc().extent);

        return temporalFilteredTex;
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Temporal failed: " + std::string(e.what()));
    }
}

std::shared_ptr<Image> RtdgiRenderer::Spatial(
    TemporalRenderGraph& rg,
    const std::shared_ptr<Image>& inputColor,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<Image>& ssaoTex,
    VkDescriptorSet bindlessDescriptorSet)
{
    try {
        auto spatialFilteredTex = rg.create(TemporalTexDesc(inputColor->desc().extent_2d()));

        SimpleRenderPass::new_compute(
            rg.add_pass("rtdgi spatial"),
            "/shaders/rtdgi/spatial_filter.hlsl"
        )
        .read(inputColor)
        .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .read(ssaoTex)
        .read(gbufferDepth.geometric_normal)
        .write(spatialFilteredTex)
        .constants(std::make_tuple(spatialFilteredTex->desc().extent_inv_extent_2d()))
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .dispatch(spatialFilteredTex->desc().extent);

        return spatialFilteredTex;
    } catch (const std::exception& e) {
        throw std::runtime_error("RtdgiRenderer::Spatial failed: " + std::string(e.what()));
    }
}

ReprojectedRtdgi RtdgiRenderer::Reproject(
    TemporalRenderGraph& rg,
    const std::shared_ptr<Image>& reprojectionMap)
{
    try {
        auto gbufferExtent = reprojectionMap->desc().extent_2d();

        auto [temporalOutputTex, historyTex] = Temporal2Tex.get_output_and_history(
            rg, TemporalTexDesc(gbufferExtent)
        );

        auto reprojectedHistoryTex = rg.create(TemporalTexDesc(gbufferExtent));

        SimpleRenderPass::new_compute(
            rg.add_pass("rtdgi reproject"),
            "/shaders/rtdgi/fullres_reproject.hlsl"
        )
        .read(historyTex)
        .read(reprojectionMap)
        .write(reprojectedHistoryTex)
        .constants(std::make_tuple(reprojectedHistoryTex->desc().extent_inv_extent_2d()))
        .dispatch(reprojectedHistoryTex->desc().extent);

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
    const std::shared_ptr<Image>& reprojectionMap,
    const std::shared_ptr<Image>& skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    const std::shared_ptr<RayTracingAcceleration>& tlas,
    const std::shared_ptr<Image>& ssaoTex)
{
    try {
        auto halfSsaoTex = rg.create(
            ssaoTex->desc()
                .half_res()
                .usage(VkImageUsageFlags(0))
                .format(VK_FORMAT_R8_SNORM)
        );

        SimpleRenderPass::new_compute(
            rg.add_pass("extract ssao/2"),
            "/shaders/extract_half_res_ssao.hlsl"
        )
        .read(ssaoTex)
        .write(halfSsaoTex)
        .dispatch(halfSsaoTex->desc().extent);

        auto gbufferDesc = gbufferDepth.gbuffer->desc();

        auto [hitNormalOutputTex, hitNormalHistoryTex] = TemporalHitNormalTex.get_output_and_history(
            rg,
            TemporalTexDesc(
                gbufferDesc
                    .format(VK_FORMAT_R8G8B8A8_UNORM)
                    .half_res()
                    .extent_2d()
            )
        );

        auto [candidateOutputTex, candidateHistoryTex] = TemporalCandidateTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(
                VK_FORMAT_R16G16B16A16_SFLOAT,
                gbufferDesc.half_res().extent_2d()
            )
            .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto candidateRadianceTex = rg.create(
            gbufferDesc
                .half_res()
                .format(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        auto candidateNormalTex = rg.create(
            gbufferDesc.half_res().format(VK_FORMAT_R8G8B8A8_SNORM)
        );

        auto candidateHitTex = rg.create(
            gbufferDesc
                .half_res()
                .format(VK_FORMAT_R16G16B16A16_SFLOAT)
        );

        auto temporalReservoirPackedTex = rg.create(
            gbufferDesc
                .half_res()
                .format(VK_FORMAT_R32G32B32A32_UINT)
        );

        auto halfDepthTex = gbufferDepth.half_depth(rg);

        auto [invalidityOutputTex, invalidityHistoryTex] = TemporalInvalidityTex.get_output_and_history(
            rg,
            TemporalTexDesc(gbufferDesc.half_res().extent_2d())
                .format(VK_FORMAT_R16G16_SFLOAT)
        );

        auto [radianceTex, temporalReservoirTex] = [&]() -> std::tuple<std::shared_ptr<Image>, std::shared_ptr<Image>> {
            auto [radianceOutputTex, radianceHistoryTex] = TemporalRadianceTex.get_output_and_history(
                rg,
                TemporalTexDesc(gbufferDesc.half_res().extent_2d())
            );

            auto [rayOrigOutputTex, rayOrigHistoryTex] = TemporalRayOrigTex.get_output_and_history(
                rg,
                ImageDesc::new_2d(
                    VK_FORMAT_R32G32B32A32_SFLOAT,
                    gbufferDesc.half_res().extent_2d()
                )
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            auto [rayOutputTex, rayHistoryTex] = TemporalRayTex.get_output_and_history(
                rg,
                TemporalTexDesc(gbufferDesc.half_res().extent_2d())
                    .format(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            auto halfViewNormalTex = gbufferDepth.half_view_normal(rg);

            auto rtHistoryValidityPreInputTex = rg.create(
                gbufferDesc.half_res().format(VK_FORMAT_R8_UNORM)
            );

            auto [reservoirOutputTex, reservoirHistoryTex] = TemporalReservoirTex.get_output_and_history(
                rg,
                ImageDesc::new_2d(VK_FORMAT_R32G32_UINT, gbufferDesc.half_res().extent_2d())
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            SimpleRenderPass::new_rt(
                rg.add_pass("rtdgi validate"),
                ShaderSource::hlsl("/shaders/rtdgi/diffuse_validate.rgen.hlsl"),
                {
                    ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .read(halfViewNormalTex)
            .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(reprojectedRtdgi.ReprojectedHistoryTex)
            .write(reservoirHistoryTex)
            .read(rayHistoryTex)
            .read(reprojectionMap)
            .bind_mut(ircache)
            .bind(wrc)
            .read(skyCube)
            .write(radianceHistoryTex)
            .read(rayOrigHistoryTex)
            .write(rtHistoryValidityPreInputTex)
            .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .trace_rays(tlas, candidateRadianceTex->desc().extent);

            auto rtHistoryValidityInputTex = rg.create(
                gbufferDesc.half_res().format(VK_FORMAT_R8_UNORM)
            );

            SimpleRenderPass::new_rt(
                rg.add_pass("rtdgi trace"),
                ShaderSource::hlsl("/shaders/rtdgi/trace_diffuse.rgen.hlsl"),
                {
                    ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .read(halfViewNormalTex)
            .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(reprojectedRtdgi.ReprojectedHistoryTex)
            .read(reprojectionMap)
            .bind_mut(ircache)
            .bind(wrc)
            .read(skyCube)
            .read(rayOrigHistoryTex)
            .write(candidateRadianceTex)
            .write(candidateNormalTex)
            .write(candidateHitTex)
            .read(rtHistoryValidityPreInputTex)
            .write(rtHistoryValidityInputTex)
            .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .trace_rays(tlas, candidateRadianceTex->desc().extent);

            SimpleRenderPass::new_compute(
                rg.add_pass("validity integrate"),
                "/shaders/rtdgi/temporal_validity_integrate.hlsl"
            )
            .read(rtHistoryValidityInputTex)
            .read(invalidityHistoryTex)
            .read(reprojectionMap)
            .read(halfViewNormalTex)
            .read(halfDepthTex)
            .write(invalidityOutputTex)
            .constants(std::make_tuple(
                gbufferDesc.extent_inv_extent_2d(),
                invalidityOutputTex->desc().extent_inv_extent_2d()
            ))
            .dispatch(invalidityOutputTex->desc().extent);

            SimpleRenderPass::new_compute(
                rg.add_pass("restir temporal"),
                "/shaders/rtdgi/restir_temporal.hlsl"
            )
            .read(halfViewNormalTex)
            .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(candidateRadianceTex)
            .read(candidateNormalTex)
            .read(candidateHitTex)
            .read(radianceHistoryTex)
            .read(rayOrigHistoryTex)
            .read(rayHistoryTex)
            .read(reservoirHistoryTex)
            .read(reprojectionMap)
            .read(hitNormalHistoryTex)
            .read(candidateHistoryTex)
            .read(invalidityOutputTex)
            .write(radianceOutputTex)
            .write(rayOrigOutputTex)
            .write(rayOutputTex)
            .write(hitNormalOutputTex)
            .write(reservoirOutputTex)
            .write(candidateOutputTex)
            .write(temporalReservoirPackedTex)
            .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .dispatch(radianceOutputTex->desc().extent);

            return std::make_tuple(radianceOutputTex, reservoirOutputTex);
        }();

        auto irradianceTex = [&]() -> std::shared_ptr<Image> {
            auto halfViewNormalTex = gbufferDepth.half_view_normal(rg);

            auto reservoirOutputTex0 = rg.create(
                gbufferDesc
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .half_res()
                    .format(VK_FORMAT_R32G32_UINT)
            );
            auto reservoirOutputTex1 = rg.create(
                gbufferDesc
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .half_res()
                    .format(VK_FORMAT_R32G32_UINT)
            );

            auto bouncedRadianceOutputTex0 = rg.create(
                gbufferDesc
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .half_res()
                    .format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            );
            auto bouncedRadianceOutputTex1 = rg.create(
                gbufferDesc
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
                    .half_res()
                    .format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            );

            auto reservoirInputTex = temporalReservoirTex;
            auto bouncedRadianceInputTex = radianceTex;

            for (uint32_t spatialReusePassIdx = 0; spatialReusePassIdx < SpatialReusePassCount; ++spatialReusePassIdx) {
                uint32_t performOcclusionRaymarch = (spatialReusePassIdx + 1 == SpatialReusePassCount) ? 1 : 0;
                uint32_t occlusionRaymarchImportanceOnly = UseRaytracedReservoirVisibility ? 1 : 0;

                SimpleRenderPass::new_compute(
                    rg.add_pass("restir spatial"),
                    "/shaders/rtdgi/restir_spatial.hlsl"
                )
                .read(reservoirInputTex)
                .read(bouncedRadianceInputTex)
                .read(halfViewNormalTex)
                .read(halfDepthTex)
                .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
                .read(halfSsaoTex)
                .read(temporalReservoirPackedTex)
                .read(reprojectedRtdgi.ReprojectedHistoryTex)
                .write(reservoirOutputTex0)
                .write(bouncedRadianceOutputTex0)
                .constants(std::make_tuple(
                    gbufferDesc.extent_inv_extent_2d(),
                    reservoirOutputTex0->desc().extent_inv_extent_2d(),
                    spatialReusePassIdx,
                    performOcclusionRaymarch,
                    occlusionRaymarchImportanceOnly
                ))
                .dispatch(reservoirOutputTex0->desc().extent);

                std::swap(reservoirOutputTex0, reservoirOutputTex1);
                std::swap(bouncedRadianceOutputTex0, bouncedRadianceOutputTex1);

                reservoirInputTex = reservoirOutputTex1;
                bouncedRadianceInputTex = bouncedRadianceOutputTex1;
            }

            if (UseRaytracedReservoirVisibility) {
                SimpleRenderPass::new_rt(
                    rg.add_pass("restir check"),
                    ShaderSource::hlsl("/shaders/rtdgi/restir_check.rgen.hlsl"),
                    {
                        ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                        ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
                    },
                    {ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
                )
                .read(halfDepthTex)
                .read(temporalReservoirPackedTex)
                .write(reservoirInputTex)
                .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
                .raw_descriptor_set(1, bindlessDescriptorSet)
                .trace_rays(tlas, candidateRadianceTex->desc().extent);
            }

            auto irradianceOutputTex = rg.create(
                gbufferDesc
                    .usage(VkImageUsageFlags(0))
                    .format(COLOR_BUFFER_FORMAT)
            );

            SimpleRenderPass::new_compute(
                rg.add_pass("restir resolve"),
                "/shaders/rtdgi/restir_resolve.hlsl"
            )
            .read(radianceTex)
            .read(reservoirInputTex)
            .read(gbufferDepth.gbuffer)
            .read_aspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(halfViewNormalTex)
            .read(halfDepthTex)
            .read(ssaoTex)
            .read(candidateRadianceTex)
            .read(candidateHitTex)
            .read(temporalReservoirPackedTex)
            .read(bouncedRadianceInputTex)
            .write(irradianceOutputTex)
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .constants(std::make_tuple(
                gbufferDesc.extent_inv_extent_2d(),
                irradianceOutputTex->desc().extent_inv_extent_2d()
            ))
            .dispatch(irradianceOutputTex->desc().extent);

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