#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "tekki/renderer/renderers/ircache_render_state.h"
#include "tekki/renderer/renderers/wrc_render_state.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;
using TemporalRenderGraph = tekki::render_graph::TemporalRenderGraph;
using RayTracingAcceleration = tekki::backend::vulkan::RayTracingAcceleration;

struct RtdgiCandidates {
    rg::Handle<Image> CandidateRadianceTex;
    rg::Handle<Image> CandidateNormalTex;
    rg::Handle<Image> CandidateHitTex;
};

struct RtdgiOutput {
    rg::Handle<Image> ScreenIrradianceTex;
    RtdgiCandidates Candidates;
};

struct ReprojectedRtdgi {
    rg::Handle<Image> ReprojectedHistoryTex;
    rg::Handle<Image> TemporalOutputTex;
};

class RtdgiRenderer {
private:
    static constexpr VkFormat COLOR_BUFFER_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

    PingPongTemporalResource TemporalRadianceTex;
    PingPongTemporalResource TemporalRayOrigTex;
    PingPongTemporalResource TemporalRayTex;
    PingPongTemporalResource TemporalReservoirTex;
    PingPongTemporalResource TemporalCandidateTex;
    PingPongTemporalResource TemporalInvalidityTex;
    PingPongTemporalResource Temporal2Tex;
    PingPongTemporalResource Temporal2VarianceTex;
    PingPongTemporalResource TemporalHitNormalTex;

    static rg::ImageDesc TemporalTexDesc(const std::array<uint32_t, 2>& extent);

    rg::Handle<Image> Temporal(
        TemporalRenderGraph& rg,
        const rg::Handle<Image>& inputColor,
        const GbufferDepth& gbufferDepth,
        const rg::Handle<Image>& reprojectionMap,
        const rg::Handle<Image>& reprojectedHistoryTex,
        const rg::Handle<Image>& rtHistoryInvalidityTex,
        rg::Handle<Image> temporalOutputTex);

    static rg::Handle<Image> Spatial(
        TemporalRenderGraph& rg,
        const rg::Handle<Image>& inputColor,
        const GbufferDepth& gbufferDepth,
        const rg::Handle<Image>& ssaoTex,
        VkDescriptorSet bindlessDescriptorSet);

public:
    uint32_t SpatialReusePassCount;
    bool UseRaytracedReservoirVisibility;

    RtdgiRenderer();

    ReprojectedRtdgi Reproject(
        TemporalRenderGraph& rg,
        const rg::Handle<Image>& reprojectionMap);

    RtdgiOutput Render(
        TemporalRenderGraph& rg,
        const ReprojectedRtdgi& reprojectedRtdgi,
        const GbufferDepth& gbufferDepth,
        const rg::Handle<Image>& reprojectionMap,
        const rg::Handle<Image>& skyCube,
        VkDescriptorSet bindlessDescriptorSet,
        IrcacheRenderState& ircache,
        const WrcRenderState& wrc,
        const rg::Handle<RayTracingAcceleration>& tlas,
        const rg::Handle<Image>& ssaoTex);
};

}