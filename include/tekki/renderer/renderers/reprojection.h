#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer::renderers {

struct GbufferDepth {
    std::shared_ptr<tekki::backend::vulkan::Image> Depth;
    std::shared_ptr<tekki::backend::vulkan::Image> GeometricNormal;
};

class Reprojection {
public:
    static std::shared_ptr<tekki::backend::vulkan::Image> CalculateReprojectionMap(
        tekki::render_graph::TemporalRenderGraph& renderGraph,
        const GbufferDepth& gbufferDepth,
        const std::shared_ptr<tekki::backend::vulkan::Image>& velocityImage
    );
};

} // namespace tekki::renderer::renderers