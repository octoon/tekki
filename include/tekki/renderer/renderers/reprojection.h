#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

class Reprojection {
public:
    static rg::Handle<rg::Image> CalculateReprojectionMap(
        tekki::render_graph::TemporalRenderGraph& renderGraph,
        const GbufferDepth& gbufferDepth,
        rg::Handle<rg::Image> velocityImage
    );
};

} // namespace tekki::renderer::renderers