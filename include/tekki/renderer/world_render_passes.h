#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/lib.h"
#include "world_frame_desc.h"
#include "tekki/renderer/renderers/deferred.h"
#include "tekki/renderer/renderers/motion_blur.h"
#include "tekki/renderer/renderers/raster_meshes.h"
#include "tekki/renderer/renderers/reference.h"
#include "tekki/renderer/renderers/shadows.h"
#include "tekki/renderer/renderers/sky.h"
#include "tekki/renderer/renderers/reprojection.h"
#include "tekki/renderer/renderers/wrc.h"
#include "tekki/renderer/renderers/ircache.h"
#include "tekki/renderer/renderers/rtdgi.h"
#include "tekki/renderer/renderers/rtr.h"
#include "tekki/renderer/renderers/lighting.h"
#include "tekki/renderer/renderers/taa.h"
#include "tekki/renderer/renderers/post.h"
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