#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/types.h"
#include "tekki/render_graph/resource.h"

namespace tekki::render_graph {

// Forward declarations
class RenderGraph;

class ImageOps {
public:
    /**
     * Clear depth image
     */
    static void ClearDepth(RenderGraph& rg, Handle<Image>& img);

    /**
     * Clear color image
     */
    static void ClearColor(RenderGraph& rg, Handle<Image>& img, const glm::vec4& clear_color);
};

} // namespace tekki::render_graph