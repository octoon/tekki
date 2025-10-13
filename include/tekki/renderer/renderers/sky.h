#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/render_graph/lib.h"
#include "tekki/render_graph/Image.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

class SkyRenderer {
public:
    /**
     * Render sky cube
     * @param rg The render graph
     * @return Handle to the created sky texture
     */
    static rg::Handle<rg::Image> RenderSkyCube(rg::RenderGraph& rg);

    /**
     * Convolve cube map
     * @param rg The render graph
     * @param input Input cube map texture
     * @return Handle to the convolved sky texture
     */
    static rg::Handle<rg::Image> ConvolveCube(
        rg::RenderGraph& rg,
        const rg::Handle<rg::Image>& input
    );
};

} // namespace tekki::renderer::renderers