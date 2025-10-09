```cpp
#include "tekki/renderer/renderers/rtr.h"
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/renderer/render_graph.h"
#include "tekki/renderer/renderers/ircache.h"
#include "tekki/renderer/renderers/rtdgi.h"
#include "tekki/renderer/renderers/wrc.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace {
    template<typename T>
    const uint8_t* as_byte_slice_unchecked(const std::vector<T>& v) {
        return reinterpret_cast<const uint8_t*>(v.data());
    }
}

RtrRenderer::RtrRenderer(const std::shared_ptr<Device>& device) {
    try {
        TemporalTex = PingPongTemporalResource("rtr.temporal");
        RayLenTex = PingPongTemporalResource("rtr.ray_len");
        TemporalIrradianceTex = PingPongTemporalResource("rtr.irradiance");
        TemporalRayOrigTex = PingPongTemporalResource("rtr.ray_orig");
        TemporalRayTex = PingPongTemporalResource("rtr.ray");
        TemporalReservoirTex = PingPongTemporalResource("rtr.reservoir");
        TemporalRngTex = PingPongTemporalResource("rtr.rng");
        TemporalHitNormalTex = PingPongTemporalResource("rtr.hit_normal");

        std::vector<uint32_t> ranking_tile_data(RANKING_TILE.begin(), RANKING_TILE.end());
        std::vector<uint32_t> scrambling_tile_data(SCRAMBLING_TILE.begin(), SCRAMBLING_TILE.end());
        std::vector<uint32_t> sobol_data(SOBOL.begin(), SOBOL.end());

        RankingTileBuf = MakeLutBuffer(device, ranking_tile_data.data(), ranking_tile_data.size() * sizeof(uint32_t), "ranking_tile_buf");
        ScamblingTileBuf = MakeLutBuffer(device, scrambling_tile_data.data(), scrambling_tile_data.size() * sizeof(uint32_t), "scrambling_tile_buf");
        SobolBuf = MakeLutBuffer(device, sobol_data.data(), sobol_data.size() * sizeof(uint32_t), "sobol_buf");

        ReuseRtdgiRays = true;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create RtrRenderer: " + std::string(e.what()));
    }
}

ImageDesc RtrRenderer::TemporalTexDesc(const glm::uvec2& extent) {
    return ImageDesc::new_2d(VK_FORMAT_R16G16B16A16_SFLOAT, extent)
        .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

std::shared_ptr<Buffer> RtrRenderer::MakeLutBuffer(
    const std::shared_ptr<Device>& device,
    const void* data,
    size_t size,
    const std::string& name
) {
    try {
        BufferDesc desc = BufferDesc::new_gpu_only(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        );
        return std::make_shared<Buffer>(device->create_buffer(desc, name, data));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create LUT buffer: " + std::string(e.what()));
    }
}

TracedRtr RtrRenderer::Trace(
    rg::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& reprojectionMap,
    const rg::Handle<Image>& skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    const rg::Handle<RayTracingAcceleration>& tlas,
    const rg::ReadOnlyHandle<Image>& rtdgiIrradiance,
    RtdgiCandidates rtdgiCandidates,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc
) {
    try {
        auto gbufferDesc = gbufferDepth.gbuffer.desc();

        auto refl0_tex = rtdgiCandidates.candidate_radiance_tex;
        auto refl1_tex = rtdgiCandidates.candidate_hit_tex;
        auto refl2_tex = rtdgiCandidates.candidate_normal_tex;

        auto ranking_tile_buf = rg.import(
            RankingTileBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );
        auto scambling_tile_buf = rg.import(
            ScamblingTileBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );
        auto sobol_buf = rg.import(
            SobolBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        auto [rng_output_tex, rng_history_tex] = TemporalRngTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(VK_FORMAT_R32_UINT, gbufferDesc.half_res().extent_2d())
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        uint32_t reuse_rtdgi_rays_u32 = ReuseRtdgiRays ? 1u : 0u;

        SimpleRenderPass::new_rt(
            rg.add_pass("reflection trace"),
            ShaderSource::hlsl("/shaders/rtr/reflection.rgen.hlsl"),
            {
                ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
        )
        .read(&gbufferDepth.gbuffer)
        .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .read(&ranking_tile_buf)
        .read(&scambling_tile_buf)
        .read(&sobol_buf)
        .read(rtdgiIrradiance)
        .read(skyCube)
        .bind_mut(&ircache)
        .bind(&wrc)
        .write(&refl0_tex)
        .write(&refl1_tex)
        .write(&refl2_tex)
        .write(&rng_output_tex)
        .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d(), reuse_rtdgi_rays_u32))
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .trace_rays(tlas, refl0_tex.desc().extent);

        auto half_view_normal_tex = gbufferDepth.half_view_normal(rg);
        auto half_depth_tex = gbufferDepth.half_depth(rg);

        auto [ray_orig_output_tex, ray_orig_history_tex] = TemporalRayOrigTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(
                VK_FORMAT_R32G32B32A32_SFLOAT,
                gbufferDesc.half_res().extent_2d()
            )
            .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto refl_restir_invalidity_tex = rg.create(refl0_tex.desc().format(VK_FORMAT_R8_UNORM));

        auto [irradiance_tex, ray_tex, temporal_reservoir_tex, restir_hit_normal_tex] = [&]() {
            auto [hit_normal_output_tex, hit_normal_history_tex] = TemporalHitNormalTex.get_output_and_history(
                rg,
                TemporalTexDesc(
                    gbufferDesc
                        .format(VK_FORMAT_R8G8B8A8_UNORM)
                        .half_res()
                        .extent_2d()
                )
            );

            auto [irradiance_output_tex, irradiance_history_tex] = TemporalIrradianceTex.get_output_and_history(
                rg,
                TemporalTexDesc(gbufferDesc.half_res().extent_2d())
                    .format(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            auto [reservoir_output_tex, reservoir_history_tex] = TemporalReservoirTex.get_output_and_history(
                rg,
                ImageDesc::new_2d(VK_FORMAT_R32G32_UINT, gbufferDesc.half_res().extent_2d())
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            auto [ray_output_tex, ray_history_tex] = TemporalRayTex.get_output_and_history(
                rg,
                TemporalTexDesc(gbufferDesc.half_res().extent_2d())
                    .format(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            SimpleRenderPass::new_rt(
                rg.add_pass("reflection validate"),
                ShaderSource::hlsl("/shaders/rtr/reflection_validate.rgen.hlsl"),
                {
                    ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .read(&gbufferDepth.gbuffer)
            .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(rtdgiIrradiance)
            .read(skyCube)
            .write(&refl_restir_invalidity_tex)
            .bind_mut(&ircache)
            .bind(&wrc)
            .read(&ray_orig_history_tex)
            .read(&ray_history_tex)
            .read(&rng_history_tex)
            .write(&irradiance_history_tex)
            .write(&reservoir_history_tex)
            .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .trace_rays(tlas, refl0_tex.desc().half_res().extent);

            SimpleRenderPass::new_compute(
                rg.add_pass("rtr restir temporal"),
                "/shaders/rtr/rtr_restir_temporal.hlsl"
            )
            .read(&gbufferDepth.gbuffer)
            .read(&*half_view_normal_tex)
            .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .read(&refl0_tex)
            .read(&refl1_tex)
            .read(&refl2_tex)
            .read(&irradiance_history_tex)
            .read(&ray_orig_history_tex)
            .read(&ray_history_tex)
            .read(&rng_history_tex)
            .read(&reservoir_history_tex)
            .read(reprojectionMap)
            .read(&hit_normal_history_tex)
            .write(&irradiance_output_tex)
            .write(&ray_orig_output_tex)
            .write(&ray_output_tex)
            .write(&rng_output_tex)
            .write(&hit_normal_output_tex)
            .write(&reservoir_output_tex)
            .constants(std::make_tuple(gbufferDesc.extent_inv_extent_2d()))
            .raw_descriptor_set(1, bindlessDescriptorSet)
            .dispatch(irradiance_output_tex.desc().extent);

            return std::make_tuple(
                irradiance_output_tex,
                ray_output_tex,
                reservoir_output_tex,
                hit_normal_output_tex
            );
        }();

        auto resolved_tex = rg.create(
            gbufferDepth
                .gbuffer
                .desc()
                .usage(VkImageUsageFlags(0))
                .format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
        );

        auto [temporal_output_tex, history_tex] = TemporalTex.get_output_and_history(rg, TemporalTexDesc(gbufferDesc.extent_2d()));

        auto [ray_len_output_tex, ray_len_history_tex] = RayLenTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(VK_FORMAT_R16G16_SFLOAT, gbufferDesc.extent_2d())
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        SimpleRenderPass::new_compute(
            rg.add_pass("reflection resolve"),
            "/shaders/rtr/resolve.hlsl"
        )
        .read(&gbufferDepth.gbuffer)
        .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .read(&refl0_tex)
        .read(&refl1_tex)
        .read(&refl2_tex)
        .read(&history_tex)
        .read(reprojectionMap)
        .read(&*half_view_normal_tex)
        .read(&*half_depth_tex)
        .read(&ray_len_history_tex)
        .read(&irradiance_tex)
        .read(&ray_tex)
        .read(&temporal_reservoir_tex)
        .read(&ray_orig_output_tex)
        .read(&restir_hit_normal_tex)
        .write(&resolved_tex)
        .write(&ray_len_output_tex)
        .raw_descriptor_set(1, bindlessDescriptorSet)
        .constants(std::make_tuple(
            resolved_tex.desc().extent_inv_extent_2d(),
            SpatialResolveOffsets
        ))
        .dispatch(resolved_tex.desc().extent);

        return TracedRtr{
            resolved_tex,
            temporal_output_tex,
            history_tex,
            ray_len_output_tex,
            refl_restir_invalidity_tex
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to trace RTR: " + std::string(e.what()));
    }
}

TracedRtr RtrRenderer::CreateDummyOutput(
    rg::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth
) {
    try {
        auto gbufferDesc = gbufferDepth.gbuffer.desc();

        auto resolved_tex = rg.create(
            gbufferDepth
                .gbuffer
                .desc()
                .usage(VkImageUsageFlags(0))
                .format(VK_FORMAT_R8G8B8A8_UNORM)
        );

        auto [temporal_output_tex, history_tex] = TemporalTex.get_output_and_history(rg, TemporalTexDesc(gbufferDesc.extent_2d()));

        auto [ray_len_output_tex, _ray_len_history_tex] = RayLenTex.get_output_and_history(
            rg,
            ImageDesc::new_2d(VK_FORMAT_R8G8B8A8_UNORM, gbufferDesc.extent_2d())
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto refl_restir_invalidity_tex = rg.create(ImageDesc::new_2d(VK_FORMAT_R8_UNORM, glm::uvec2(1, 1)));

        return TracedRtr{
            resolved_tex,
            temporal_output_tex,
            history_tex,
            ray_len_output_tex,
            refl_restir_invalidity_tex
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create dummy RTR output: " + std::string(e.what()));
    }
}

rg::Handle<Image> TracedRtr::FilterTemporal(
    rg::TemporalRenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const rg::Handle<Image>& reprojectionMap
) {
    try {
        SimpleRenderPass::new_compute(
            rg.add_pass("reflection temporal"),
            "/shaders/rtr/temporal_filter.hlsl"
        )
        .read(&ResolvedTex)
        .read(&HistoryTex)
        .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .read(&RayLenTex)
        .read(reprojectionMap)
        .read(&ReflRestirInvalidityTex)
        .read(&gbufferDepth.gbuffer)
        .write(&TemporalOutputTex)
        .constants(TemporalOutputTex.desc().extent_inv_extent_2d())
        .dispatch(ResolvedTex.desc().extent);

        SimpleRenderPass::new_compute(
            rg.add_pass("reflection cleanup"),
            "/shaders/rtr/spatial_cleanup.hlsl"
        )
        .read(&TemporalOutputTex)
        .read_aspect(&gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .read(&gbufferDepth.geometric_normal)
        .write(&ResolvedTex)
        .constants(SpatialResolveOffsets)
        .dispatch(ResolvedTex.desc().extent);

        return ResolvedTex;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to filter RTR temporally: " + std::string(e.what()));
    }
}

const std::array<glm::ivec4, 16 * 4 * 8> SpatialResolveOffsets = {
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, -1, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(0, -3, 0, 0),
    glm::ivec4(-3, 0, 0, 0),
    glm::ivec4(3, 0, 0, 0),
    glm::ivec4(3, 1, 0, 0),
    glm::ivec4(-1, 3, 0, 0),
    glm::ivec4(1, 3, 0, 0),
    glm::ivec4(2, -3, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(1, 4, 0, 0),
    glm::ivec4(-3, 3, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(-1, 1, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(1, 2, 0, 0),
    glm::ivec4(2, -2, 0, 0),
    glm::ivec4(1, -3, 0, 0),
    glm::ivec4(-4, 0, 0, 0),
    glm::ivec4(4, 0, 0, 0),
    glm::ivec4(-1, -4, 0, 0),
    glm::ivec4(-4, -1, 0, 0),
    glm::ivec4(-4, 1, 0, 0),
    glm::ivec4(-3, -3, 0, 0),
    glm::ivec4(3, 3, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, -1, 0, 0),
    glm::ivec4(-1, 0, 0, 0),
    glm::ivec4(1, -1, 0, 0),
    glm::ivec4(-2, -1, 0, 0),
    glm::ivec4(2, 1, 0, 0),
    glm::ivec4(-1, 2, 0, 0),
    glm::ivec4(0, 3, 0, 0),
    glm::ivec4(3, -1, 0, 0),
    glm::ivec4(-2, -3, 0, 0),
    glm::ivec4(3, -2, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(0, -4, 0, 0),
    glm::ivec4(4, 1, 0, 0),
    glm::ivec4(-1, 4, 0, 0),
    glm::ivec4(3, -3, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(1, 0, 0, 0),
    glm::ivec4(-2, 0, 0, 0),
    glm::ivec4(2, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(1, -2, 0, 0),
    glm::ivec4(-1, -3, 0, 0),
    glm::ivec4(-3, -1, 0, 0),
    glm::ivec4(-3, 1, 0, 0),
    glm::ivec4(3, 2, 0, 0),
    glm::ivec4(-2, 3, 0, 0),
    glm::ivec4(2, 3, 0, 0),
    glm::ivec4(0, 4, 0, 0),
    glm::ivec4(1, -4, 0, 0),
    glm::ivec4(4, -1, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(-1, -1, 0, 0),
    glm::ivec4(1, -2, 0, 0),
    glm::ivec4(2, -2, 0, 0),
    glm::ivec4(3, 0, 0, 0),
    glm::ivec4(1, 3, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(-4, 0, 0, 0),
    glm::ivec4(-1, -4, 0, 0),
    glm::ivec4(-1, 4, 0, 0),
    glm::ivec4(-3, -3, 0, 0),
    glm::ivec4(4, 2, 0, 0),
    glm::ivec4(1, -5, 0, 0),
    glm::ivec4(-4, -4, 0, 0),
    glm::ivec4(4, -4, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, -1, 0, 0),
    glm::ivec4(1, 0, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(0, 3, 0, 0),
    glm::ivec4(-3, -1, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(0, -4, 0, 0),
    glm::ivec4(4, -1, 0, 0),
    glm::ivec4(-3, 3, 0, 0),
    glm::ivec4(3, 3, 0, 0),
    glm::ivec4(-2, 4, 0, 0),
    glm::ivec4(3, -4, 0, 0),
    glm::ivec4(5, -1, 0, 0),
    glm::ivec4(-2, -5, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, 1, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(2, 1, 0, 0),
    glm::ivec4(0, -3, 0, 0),
    glm::ivec4(3, -1, 0, 0),
    glm::ivec4(-3, 1, 0, 0),
    glm::ivec4(-1, 3, 0, 0),
    glm::ivec4(3, -2, 0, 0),
    glm::ivec4(1, 4, 0, 0),
    glm::ivec4(3, -3, 0, 0),
    glm::ivec4(-4, -2, 0, 0),
    glm::ivec4(0, -5, 0, 0),
    glm