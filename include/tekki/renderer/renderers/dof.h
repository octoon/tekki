#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "render_graph.h"

namespace tekki::renderer::renderers {

class Dof {
public:
    /**
     * Apply depth of field effect
     * @param renderGraph The render graph
     * @param input Input image handle
     * @param depth Depth image handle
     * @return Processed image handle with depth of field applied
     */
    static std::shared_ptr<tekki::backend::vulkan::Image> Apply(
        std::shared_ptr<RenderGraph> renderGraph,
        std::shared_ptr<tekki::backend::vulkan::Image> input,
        std::shared_ptr<tekki::backend::vulkan::Image> depth
    );
};

} // namespace tekki::renderer::renderers