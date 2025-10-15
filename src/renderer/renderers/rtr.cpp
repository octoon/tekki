#include "tekki/renderer/renderers/rtr.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "tekki/backend/vulkan/shader.h"
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/render_graph/lib.h"
#include "tekki/renderer/renderers/ircache.h"
#include "tekki/renderer/renderers/rtdgi.h"
#include "tekki/renderer/renderers/wrc.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;
using ShaderSource = tekki::backend::vulkan::ShaderSource;
using RayTracingAcceleration = tekki::backend::vulkan::RayTracingAcceleration;
// #include "tekki/rust_shaders_shared/blue_noise.h"  // TODO: Need to create this file with blue noise data

namespace tekki::renderer::renderers {

// Blue noise sampling data - these will be properly initialized from blue_noise_sampler crate data
// For now, using empty arrays as placeholders
static const std::array<uint32_t, 128 * 128 * 8> RANKING_TILE_DATA = {};
static const std::array<uint32_t, 128 * 128 * 8> SCRAMBLING_TILE_DATA = {};
static const std::array<uint32_t, 256> SOBOL_DATA = {};

namespace {
    template<typename T>
    const uint8_t* as_byte_slice_unchecked(const std::vector<T>& v) {
        return reinterpret_cast<const uint8_t*>(v.data());
    }
}

RtrRenderer::RtrRenderer(const std::shared_ptr<tekki::backend::vulkan::Device>& device) :
    TemporalTex("rtr.temporal"),
    RayLenTex("rtr.ray_len"),
    TemporalIrradianceTex("rtr.irradiance"),
    TemporalRayOrigTex("rtr.ray_orig"),
    TemporalRayTex("rtr.ray"),
    TemporalReservoirTex("rtr.reservoir"),
    TemporalRngTex("rtr.rng"),
    TemporalHitNormalTex("rtr.hit_normal")
{
    try {
        std::vector<uint32_t> ranking_tile_data(RANKING_TILE_DATA.begin(), RANKING_TILE_DATA.end());
        std::vector<uint32_t> scrambling_tile_data(SCRAMBLING_TILE_DATA.begin(), SCRAMBLING_TILE_DATA.end());
        std::vector<uint32_t> sobol_data(SOBOL_DATA.begin(), SOBOL_DATA.end());

        RankingTileBuf = MakeLutBuffer(device, ranking_tile_data.data(), ranking_tile_data.size() * sizeof(uint32_t), "ranking_tile_buf");
        ScamblingTileBuf = MakeLutBuffer(device, scrambling_tile_data.data(), scrambling_tile_data.size() * sizeof(uint32_t), "scrambling_tile_buf");
        SobolBuf = MakeLutBuffer(device, sobol_data.data(), sobol_data.size() * sizeof(uint32_t), "sobol_buf");

        ReuseRtdgiRays = true;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create RtrRenderer: " + std::string(e.what()));
    }
}

tekki::render_graph::ImageDesc RtrRenderer::TemporalTexDesc(const glm::uvec2& extent) {
    return tekki::render_graph::ImageDesc::New2d(VK_FORMAT_R16G16B16A16_SFLOAT, extent)
        .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

std::shared_ptr<tekki::backend::vulkan::Buffer> RtrRenderer::MakeLutBuffer(
    const std::shared_ptr<tekki::backend::vulkan::Device>& device,
    const void* data,
    size_t size,
    const std::string& name
) {
    try {
        tekki::backend::vulkan::BufferDesc desc = tekki::backend::vulkan::BufferDesc::NewGpuOnly(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        );

        // Convert void* to vector<uint8_t>
        std::vector<uint8_t> dataVec(static_cast<const uint8_t*>(data),
                                      static_cast<const uint8_t*>(data) + size);

        return std::make_shared<tekki::backend::vulkan::Buffer>(
            device->CreateBuffer(desc, name, dataVec)
        );
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
        auto gbufferDesc = gbufferDepth.gbuffer.desc;

        auto refl0_tex = rtdgiCandidates.CandidateRadianceTex;
        auto refl1_tex = rtdgiCandidates.CandidateHitTex;
        auto refl2_tex = rtdgiCandidates.CandidateNormalTex;

        auto ranking_tile_buf = rg.GetRenderGraph()->Import(
            RankingTileBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );
        auto scambling_tile_buf = rg.GetRenderGraph()->Import(
            ScamblingTileBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );
        auto sobol_buf = rg.GetRenderGraph()->Import(
            SobolBuf,
            vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        auto [rng_output_tex, rng_history_tex] = TemporalRngTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(VK_FORMAT_R32_UINT, gbufferDesc.WithHalfRes().GetExtent2d())
                .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        uint32_t reuse_rtdgi_rays_u32 = ReuseRtdgiRays ? 1u : 0u;

        auto pass1 = rg.AddPass("reflection trace");
        rg::SimpleRenderPass::NewRt(
            pass1,
            ShaderSource::CreateHlsl("/shaders/rtr/reflection.rgen.hlsl"),
            {
                ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
            },
            {ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")}
        )
        .Read(gbufferDepth.gbuffer)
        .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(ranking_tile_buf)
        .Read(scambling_tile_buf)
        .Read(sobol_buf)
        .Read(rtdgiIrradiance)
        .Read(skyCube)
        .Write(refl0_tex)
        .Write(refl1_tex)
        .Write(refl2_tex)
        .Write(rng_output_tex)
        .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d(), reuse_rtdgi_rays_u32))
        .RawDescriptorSet(1, bindlessDescriptorSet)
        .TraceRays(tlas, refl0_tex.desc.Extent);

        auto half_view_normal_tex = gbufferDepth.half_view_normal(*rg.GetRenderGraph());
        auto half_depth_tex = gbufferDepth.half_depth(*rg.GetRenderGraph());

        auto [ray_orig_output_tex, ray_orig_history_tex] = TemporalRayOrigTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(
                VK_FORMAT_R32G32B32A32_SFLOAT,
                gbufferDesc.WithHalfRes().GetExtent2d()
            )
            .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto refl_restir_invalidity_tex = rg.Create(refl0_tex.desc.WithFormat(VK_FORMAT_R8_UNORM));

        auto [irradiance_tex, ray_tex, temporal_reservoir_tex, restir_hit_normal_tex] = [&]() {
            auto [hit_normal_output_tex, hit_normal_history_tex] = TemporalHitNormalTex.GetOutputAndHistory(
                rg,
                TemporalTexDesc(
                    gbufferDesc
                        .WithFormat(VK_FORMAT_R8G8B8A8_UNORM)
                        .WithHalfRes()
                        .GetExtent2d()
                )
            );

            auto [irradiance_output_tex, irradiance_history_tex] = TemporalIrradianceTex.GetOutputAndHistory(
                rg,
                TemporalTexDesc(gbufferDesc.WithHalfRes().GetExtent2d())
                    .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            auto [reservoir_output_tex, reservoir_history_tex] = TemporalReservoirTex.GetOutputAndHistory(
                rg,
                rg::ImageDesc::New2d(VK_FORMAT_R32G32_UINT, gbufferDesc.WithHalfRes().GetExtent2d())
                    .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
            );

            auto [ray_output_tex, ray_history_tex] = TemporalRayTex.GetOutputAndHistory(
                rg,
                TemporalTexDesc(gbufferDesc.WithHalfRes().GetExtent2d())
                    .WithFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            );

            auto pass2 = rg.AddPass("reflection validate");
            rg::SimpleRenderPass::NewRt(
                pass2,
                ShaderSource::CreateHlsl("/shaders/rtr/reflection_validate.rgen.hlsl"),
                {
                    ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
                    ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
                },
                {ShaderSource::CreateHlsl("/shaders/rt/gbuffer.rchit.hlsl")}
            )
            .Read(gbufferDepth.gbuffer)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(rtdgiIrradiance)
            .Read(skyCube)
            .Write(refl_restir_invalidity_tex)
            .BindMut(ircache)
            .Bind(wrc)
            .Read(ray_orig_history_tex)
            .Read(ray_history_tex)
            .Read(rng_history_tex)
            .Write(irradiance_history_tex)
            .Write(reservoir_history_tex)
            .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .TraceRays(tlas, rg::ImageDesc(refl0_tex.desc).WithHalfRes().Extent);

            auto pass3 = rg.AddPass("rtr restir temporal");
            rg::SimpleRenderPass::NewCompute(
                pass3,
                "/shaders/rtr/rtr_restir_temporal.hlsl"
            )
            .Read(gbufferDepth.gbuffer)
            .Read(half_view_normal_tex)
            .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(refl0_tex)
            .Read(refl1_tex)
            .Read(refl2_tex)
            .Read(irradiance_history_tex)
            .Read(ray_orig_history_tex)
            .Read(ray_history_tex)
            .Read(rng_history_tex)
            .Read(reservoir_history_tex)
            .Read(reprojectionMap)
            .Read(hit_normal_history_tex)
            .Write(irradiance_output_tex)
            .Write(ray_orig_output_tex)
            .Write(ray_output_tex)
            .Write(rng_output_tex)
            .Write(hit_normal_output_tex)
            .Write(reservoir_output_tex)
            .Constants(std::make_tuple(gbufferDesc.GetExtentInvExtent2d()))
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Dispatch(irradiance_output_tex.desc.Extent);

            return std::make_tuple(
                irradiance_output_tex,
                ray_output_tex,
                reservoir_output_tex,
                hit_normal_output_tex
            );
        }();

        auto resolved_tex = rg.Create(
            gbufferDepth
                .gbuffer
                .desc
                .WithUsage(VkImageUsageFlags(0))
                .WithFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
        );

        auto [temporal_output_tex, history_tex] = TemporalTex.GetOutputAndHistory(rg, TemporalTexDesc(gbufferDesc.GetExtent2d()));

        auto [ray_len_output_tex, ray_len_history_tex] = RayLenTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(VK_FORMAT_R16G16_SFLOAT, gbufferDesc.GetExtent2d())
                .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto pass4 = rg.AddPass("reflection resolve");
        rg::SimpleRenderPass::NewCompute(
            pass4,
            "/shaders/rtr/resolve.hlsl"
        )
        .Read(gbufferDepth.gbuffer)
        .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(refl0_tex)
        .Read(refl1_tex)
        .Read(refl2_tex)
        .Read(history_tex)
        .Read(reprojectionMap)
        .Read(half_view_normal_tex)
        .Read(half_depth_tex)
        .Read(ray_len_history_tex)
        .Read(irradiance_tex)
        .Read(ray_tex)
        .Read(temporal_reservoir_tex)
        .Read(ray_orig_output_tex)
        .Read(restir_hit_normal_tex)
        .Write(resolved_tex)
        .Write(ray_len_output_tex)
        .RawDescriptorSet(1, bindlessDescriptorSet)
        .Constants(std::make_tuple(
            resolved_tex.desc.GetExtentInvExtent2d(),
            SpatialResolveOffsets
        ))
        .Dispatch(resolved_tex.desc.Extent);

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
        auto gbufferDesc = gbufferDepth.gbuffer.desc;

        auto resolved_tex = rg.Create(
            gbufferDepth
                .gbuffer
                .desc
                .WithUsage(VkImageUsageFlags(0))
                .WithFormat(VK_FORMAT_R8G8B8A8_UNORM)
        );

        auto [temporal_output_tex, history_tex] = TemporalTex.GetOutputAndHistory(rg, TemporalTexDesc(gbufferDesc.GetExtent2d()));

        auto [ray_len_output_tex, _ray_len_history_tex] = RayLenTex.GetOutputAndHistory(
            rg,
            rg::ImageDesc::New2d(VK_FORMAT_R8G8B8A8_UNORM, gbufferDesc.GetExtent2d())
                .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        auto refl_restir_invalidity_tex = rg.Create(rg::ImageDesc::New2d(VK_FORMAT_R8_UNORM, glm::uvec2(1, 1)));

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
        auto pass1 = rg.AddPass("reflection temporal");
        rg::SimpleRenderPass::NewCompute(
            pass1,
            "/shaders/rtr/temporal_filter.hlsl"
        )
        .Read(ResolvedTex)
        .Read(HistoryTex)
        .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(RayLenTex)
        .Read(reprojectionMap)
        .Read(ReflRestirInvalidityTex)
        .Read(gbufferDepth.gbuffer)
        .Write(TemporalOutputTex)
        .Constants(TemporalOutputTex.desc.GetExtentInvExtent2d())
        .Dispatch(ResolvedTex.desc.Extent);

        auto pass2 = rg.AddPass("reflection cleanup");
        rg::SimpleRenderPass::NewCompute(
            pass2,
            "/shaders/rtr/spatial_cleanup.hlsl"
        )
        .Read(TemporalOutputTex)
        .ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(gbufferDepth.geometric_normal)
        .Write(ResolvedTex)
        .Constants(SpatialResolveOffsets)
        .Dispatch(ResolvedTex.desc.Extent);

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
    glm::ivec4(4, 3, 0, 0),
    glm::ivec4(-5, -1, 0, 0),
    glm::ivec4(5, 1, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, 0, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(-2, 0, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(1, 2, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(0, 4, 0, 0),
    glm::ivec4(1, -4, 0, 0),
    glm::ivec4(-4, 1, 0, 0),
    glm::ivec4(4, 1, 0, 0),
    glm::ivec4(-2, -4, 0, 0),
    glm::ivec4(2, -4, 0, 0),
    glm::ivec4(-4, 2, 0, 0),
    glm::ivec4(-4, -3, 0, 0),
    glm::ivec4(-3, 4, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, -1, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(2, -2, 0, 0),
    glm::ivec4(-3, 0, 0, 0),
    glm::ivec4(3, 1, 0, 0),
    glm::ivec4(-1, 3, 0, 0),
    glm::ivec4(-1, 4, 0, 0),
    glm::ivec4(-2, -4, 0, 0),
    glm::ivec4(5, 0, 0, 0),
    glm::ivec4(-4, 3, 0, 0),
    glm::ivec4(3, 4, 0, 0),
    glm::ivec4(-5, -1, 0, 0),
    glm::ivec4(2, -5, 0, 0),
    glm::ivec4(4, -4, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(1, -1, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(1, 2, 0, 0),
    glm::ivec4(1, -3, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(4, 0, 0, 0),
    glm::ivec4(4, 2, 0, 0),
    glm::ivec4(2, 4, 0, 0),
    glm::ivec4(3, -4, 0, 0),
    glm::ivec4(-1, -5, 0, 0),
    glm::ivec4(-5, -3, 0, 0),
    glm::ivec4(-3, 5, 0, 0),
    glm::ivec4(6, -2, 0, 0),
    glm::ivec4(-6, 2, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(3, -1, 0, 0),
    glm::ivec4(-4, 0, 0, 0),
    glm::ivec4(1, 4, 0, 0),
    glm::ivec4(3, 3, 0, 0),
    glm::ivec4(4, -2, 0, 0),
    glm::ivec4(0, -5, 0, 0),
    glm::ivec4(-3, -4, 0, 0),
    glm::ivec4(1, -5, 0, 0),
    glm::ivec4(-5, 1, 0, 0),
    glm::ivec4(-1, 5, 0, 0),
    glm::ivec4(1, 5, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(2, 1, 0, 0),
    glm::ivec4(0, -3, 0, 0),
    glm::ivec4(-3, -1, 0, 0),
    glm::ivec4(2, -3, 0, 0),
    glm::ivec4(4, -1, 0, 0),
    glm::ivec4(-3, -3, 0, 0),
    glm::ivec4(-4, 2, 0, 0),
    glm::ivec4(-2, 4, 0, 0),
    glm::ivec4(5, 2, 0, 0),
    glm::ivec4(4, 4, 0, 0),
    glm::ivec4(5, -3, 0, 0),
    glm::ivec4(-5, 3, 0, 0),
    glm::ivec4(3, 5, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(1, -1, 0, 0),
    glm::ivec4(0, -3, 0, 0),
    glm::ivec4(-3, 0, 0, 0),
    glm::ivec4(3, 1, 0, 0),
    glm::ivec4(0, 4, 0, 0),
    glm::ivec4(-4, -3, 0, 0),
    glm::ivec4(-4, 3, 0, 0),
    glm::ivec4(4, 3, 0, 0),
    glm::ivec4(5, -1, 0, 0),
    glm::ivec4(-3, -5, 0, 0),
    glm::ivec4(3, -5, 0, 0),
    glm::ivec4(5, -3, 0, 0),
    glm::ivec4(-3, 5, 0, 0),
    glm::ivec4(-6, 0, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-2, -1, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(1, 3, 0, 0),
    glm::ivec4(-4, 1, 0, 0),
    glm::ivec4(4, 1, 0, 0),
    glm::ivec4(3, -3, 0, 0),
    glm::ivec4(-1, -5, 0, 0),
    glm::ivec4(2, -5, 0, 0),
    glm::ivec4(-2, 5, 0, 0),
    glm::ivec4(2, 5, 0, 0),
    glm::ivec4(0, -6, 0, 0),
    glm::ivec4(-6, -1, 0, 0),
    glm::ivec4(6, 2, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(1, -3, 0, 0),
    glm::ivec4(3, -1, 0, 0),
    glm::ivec4(3, 2, 0, 0),
    glm::ivec4(-1, -4, 0, 0),
    glm::ivec4(-4, -2, 0, 0),
    glm::ivec4(-2, 4, 0, 0),
    glm::ivec4(5, -2, 0, 0),
    glm::ivec4(-4, -4, 0, 0),
    glm::ivec4(3, 5, 0, 0),
    glm::ivec4(0, 6, 0, 0),
    glm::ivec4(-6, -2, 0, 0),
    glm::ivec4(-6, 2, 0, 0),
    glm::ivec4(4, -5, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, -1, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(-2, 3, 0, 0),
    glm::ivec4(1, 4, 0, 0),
    glm::ivec4(-3, -4, 0, 0),
    glm::ivec4(4, -3, 0, 0),
    glm::ivec4(5, 0, 0, 0),
    glm::ivec4(1, -5, 0, 0),
    glm::ivec4(-5, -1, 0, 0),
    glm::ivec4(-5, 2, 0, 0),
    glm::ivec4(5, 3, 0, 0),
    glm::ivec4(6, 1, 0, 0),
    glm::ivec4(1, 6, 0, 0),
    glm::ivec4(-4, 6, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(1, -3, 0, 0),
    glm::ivec4(1, 3, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(4, -1, 0, 0),
    glm::ivec4(3, -4, 0, 0),
    glm::ivec4(-5, 0, 0, 0),
    glm::ivec4(-1, -5, 0, 0),
    glm::ivec4(5, 2, 0, 0),
    glm::ivec4(-2, 5, 0, 0),
    glm::ivec4(-4, -4, 0, 0),
    glm::ivec4(1, 6, 0, 0),
    glm::ivec4(-5, 5, 0, 0),
    glm::ivec4(3, 7, 0, 0),
    glm::ivec4(-8, 1, 0, 0),
    glm::ivec4(7, 4, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, -1, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(-1, 2, 0, 0),
    glm::ivec4(3, 1, 0, 0),
    glm::ivec4(-1, -4, 0, 0),
    glm::ivec4(4, 2, 0, 0),
    glm::ivec4(2, 4, 0, 0),
    glm::ivec4(1, -5, 0, 0),
    glm::ivec4(5, 1, 0, 0),
    glm::ivec4(-5, -2, 0, 0),
    glm::ivec4(-2, 5, 0, 0),
    glm::ivec4(-3, -5, 0, 0),
    glm::ivec4(3, -5, 0, 0),
    glm::ivec4(-6, -2, 0, 0),
    glm::ivec4(-6, 3, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(1, 0, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-2, -1, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(-1, 3, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(3, -2, 0, 0),
    glm::ivec4(0, 4, 0, 0),
    glm::ivec4(-4, 1, 0, 0),
    glm::ivec4(4, 1, 0, 0),
    glm::ivec4(-2, -4, 0, 0),
    glm::ivec4(-4, -3, 0, 0),
    glm::ivec4(4, -3, 0, 0),
    glm::ivec4(-1, 5, 0, 0),
    glm::ivec4(5, 2, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(-1, 1, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(1, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(3, 0, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(-2, 3, 0, 0),
    glm::ivec4(-4, -1, 0, 0),
    glm::ivec4(-4, 2, 0, 0),
    glm::ivec4(5, -1, 0, 0),
    glm::ivec4(-1, -5, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(1, -2, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(0, 3, 0, 0),
    glm::ivec4(-3, 0, 0, 0),
    glm::ivec4(3, 0, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(-3, 2, 0, 0),
    glm::ivec4(3, 2, 0, 0),
    glm::ivec4(-1, 4, 0, 0),
    glm::ivec4(1, 4, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(-1, 0, 0, 0),
    glm::ivec4(1, 0, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-2, 0, 0, 0),
    glm::ivec4(2, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(2, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(0, -3, 0, 0),
    glm::ivec4(-3, 0, 0, 0),
    glm::ivec4(3, 0, 0, 0),
    glm::ivec4(0, 3, 0, 0),
    glm::ivec4(-3, -2, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, -1, 0, 0),
    glm::ivec4(-1, -1, 0, 0),
    glm::ivec4(1, -1, 0, 0),
    glm::ivec4(-1, 1, 0, 0),
    glm::ivec4(1, 1, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-2, 0, 0, 0),
    glm::ivec4(2, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(-2, -2, 0, 0),
    glm::ivec4(2, -2, 0, 0),
    glm::ivec4(-2, 2, 0, 0),
    glm::ivec4(2, 2, 0, 0),
    glm::ivec4(-2, -3, 0, 0),
    glm::ivec4(3, -2, 0, 0),
    glm::ivec4(0, 0, 0, 0),
    glm::ivec4(0, 1, 0, 0),
    glm::ivec4(-1, 0, 0, 0),
    glm::ivec4(1, 0, 0, 0),
    glm::ivec4(0, -2, 0, 0),
    glm::ivec4(-2, 0, 0, 0),
    glm::ivec4(2, 0, 0, 0),
    glm::ivec4(0, 2, 0, 0),
    glm::ivec4(-1, -2, 0, 0),
    glm::ivec4(1, -2, 0, 0),
    glm::ivec4(-2, -1, 0, 0),
    glm::ivec4(2, -1, 0, 0),
    glm::ivec4(-2, 1, 0, 0),
    glm::ivec4(2, 1, 0, 0),
    glm::ivec4(-1, 2, 0, 0),
    glm::ivec4(1, 2, 0, 0)
};

} // namespace tekki::renderer::renderers