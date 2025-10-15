#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/renderer/renderers/ircache.h"
#include "tekki/renderer/renderers/rtdgi.h"
#include "tekki/renderer/renderers/wrc.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

struct TracedRtr {
    rg::Handle<Image> ResolvedTex;
    rg::Handle<Image> TemporalOutputTex;
    rg::Handle<Image> HistoryTex;
    rg::Handle<Image> RayLenTex;
    rg::Handle<Image> ReflRestirInvalidityTex;

    rg::Handle<Image> FilterTemporal(
        rg::TemporalRenderGraph& rg,
        const GbufferDepth& gbufferDepth,
        const rg::Handle<Image>& reprojectionMap
    );
};

class RtrRenderer {
private:
    PingPongTemporalResource TemporalTex;
    PingPongTemporalResource RayLenTex;
    PingPongTemporalResource TemporalIrradianceTex;
    PingPongTemporalResource TemporalRayOrigTex;
    PingPongTemporalResource TemporalRayTex;
    PingPongTemporalResource TemporalReservoirTex;
    PingPongTemporalResource TemporalRngTex;
    PingPongTemporalResource TemporalHitNormalTex;

    std::shared_ptr<tekki::backend::vulkan::Buffer> RankingTileBuf;
    std::shared_ptr<tekki::backend::vulkan::Buffer> ScamblingTileBuf;
    std::shared_ptr<tekki::backend::vulkan::Buffer> SobolBuf;

    bool ReuseRtdgiRays;

    static tekki::render_graph::ImageDesc TemporalTexDesc(const glm::uvec2& extent);
    static std::shared_ptr<tekki::backend::vulkan::Buffer> MakeLutBuffer(
        const std::shared_ptr<tekki::backend::vulkan::Device>& device,
        const void* data,
        size_t size,
        const std::string& name
    );

public:
    RtrRenderer(const std::shared_ptr<tekki::backend::vulkan::Device>& device);

    TracedRtr Trace(
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
    );

    TracedRtr CreateDummyOutput(
        rg::TemporalRenderGraph& rg,
        const GbufferDepth& gbufferDepth
    );

    bool GetReuseRtdgiRays() const { return ReuseRtdgiRays; }
    void SetReuseRtdgiRays(bool value) { ReuseRtdgiRays = value; }
};

extern const std::array<glm::ivec4, 16 * 4 * 8> SpatialResolveOffsets;

}