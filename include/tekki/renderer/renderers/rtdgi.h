#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/render_graph/temporal.h"
// TODO: Implement simple_render_pass.h when needed
// #include "tekki/renderer/render_graph/simple_render_pass.h"
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
    std::shared_ptr<Image> CandidateRadianceTex;
    std::shared_ptr<Image> CandidateNormalTex;
    std::shared_ptr<Image> CandidateHitTex;
};

struct RtdgiOutput {
    std::shared_ptr<Image> ScreenIrradianceTex;
    RtdgiCandidates Candidates;
};

struct ReprojectedRtdgi {
    std::shared_ptr<Image> ReprojectedHistoryTex;
    std::shared_ptr<Image> TemporalOutputTex;
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

    static Image::Desc TemporalTexDesc(const glm::uvec2& extent);

    std::shared_ptr<Image> Temporal(
        TemporalRenderGraph& rg,
        const std::shared_ptr<Image>& inputColor,
        const GbufferDepth& gbufferDepth,
        const std::shared_ptr<Image>& reprojectionMap,
        const std::shared_ptr<Image>& reprojectedHistoryTex,
        const std::shared_ptr<Image>& rtHistoryInvalidityTex,
        std::shared_ptr<Image> temporalOutputTex);

    static std::shared_ptr<Image> Spatial(
        TemporalRenderGraph& rg,
        const std::shared_ptr<Image>& inputColor,
        const GbufferDepth& gbufferDepth,
        const std::shared_ptr<Image>& ssaoTex,
        VkDescriptorSet bindlessDescriptorSet);

public:
    uint32_t SpatialReusePassCount;
    bool UseRaytracedReservoirVisibility;

    RtdgiRenderer();

    ReprojectedRtdgi Reproject(
        TemporalRenderGraph& rg,
        const std::shared_ptr<Image>& reprojectionMap);

    RtdgiOutput Render(
        TemporalRenderGraph& rg,
        const ReprojectedRtdgi& reprojectedRtdgi,
        const GbufferDepth& gbufferDepth,
        const std::shared_ptr<Image>& reprojectionMap,
        const std::shared_ptr<Image>& skyCube,
        VkDescriptorSet bindlessDescriptorSet,
        IrcacheRenderState& ircache,
        const WrcRenderState& wrc,
        const std::shared_ptr<RayTracingAcceleration>& tlas,
        const std::shared_ptr<Image>& ssaoTex);
};

}