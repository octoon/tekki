#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/graph.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

class MotionBlur {
public:
    // TODO: Implement motion blur rendering once SimpleRenderPass API is available
    // Original Rust: kajiya/src/renderers/motion_blur.rs
    static rg::Handle<Image> Apply(
        rg::RenderGraph& renderGraph,
        const rg::Handle<Image>& input,
        const rg::Handle<Image>& depth,
        const rg::Handle<Image>& reprojectionMap
    );
};

} // namespace tekki::renderer::renderers
