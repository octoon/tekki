#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/lib.h"
#include "world_frame_desc.h"
#include "renderers/deferred/light_gbuffer.h"
#include "renderers/motion_blur/motion_blur.h"
#include "renderers/raster_meshes/raster_meshes.h"
#include "renderers/reference/reference_path_trace.h"
#include "renderers/shadows/trace_sun_shadow_mask.h"
#include "renderers/sky/sky.h"
#include "renderers/reprojection/reprojection.h"
#include "renderers/wrc/wrc.h"
#include "renderers/ircache/ircache.h"
#include "renderers/rtdgi/rtdgi.h"
#include "renderers/rtr/rtr.h"
#include "renderers/lighting/lighting.h"
#include "renderers/taa/taa.h"
#include "renderers/post/post.h"
#include "world_renderer.h"

namespace tekki::renderer {

class WorldRenderer {
public:
    std::shared_ptr<Image> PrepareRenderGraphStandard(
        std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph,
        const WorldFrameDesc& frameDesc
    );

    std::shared_ptr<Image> PrepareRenderGraphReference(
        std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph,
        const WorldFrameDesc& frameDesc
    );

private:
    std::shared_ptr<Image> PrepareTopLevelAcceleration(std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph);
    
    std::shared_ptr<IblRenderer> Ibl;
    std::shared_ptr<RasterSimpleRenderPass> RasterSimpleRenderPass;
    std::vector<Mesh> Meshes;
    std::vector<MeshInstance> Instances;
    std::shared_ptr<Buffer> VertexBuffer;
    std::shared_ptr<DescriptorSet> BindlessDescriptorSet;
    std::shared_ptr<SsgiRenderer> Ssgi;
    std::shared_ptr<IrcacheState> Ircache;
    std::shared_ptr<RtdgiRenderer> Rtdgi;
    std::shared_ptr<ShadowDenoiseRenderer> ShadowDenoise;
    std::shared_ptr<RtrRenderer> Rtr;
    std::shared_ptr<LightingRenderer> Lighting;
    std::shared_ptr<TaaRenderer> Taa;
    std::shared_ptr<PostRenderer> Post;
    std::vector<MeshLight> MeshLights;
    float SunSizeMultiplier;
    RenderDebugMode DebugMode;
    bool DebugShowWrc;
    bool UseDlss;
    glm::u32vec2 TemporalUpscaleExtent;
    bool ResetReferenceAccumulation;
    DynamicExposureState DynamicExposure;
    float Contrast;
    DebugShadingMode DebugShadingMode;
};

} // namespace tekki::renderer