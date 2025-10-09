#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer::renderers {

class SkyRenderer {
public:
    /**
     * Render sky cube
     * @param rg The render graph
     * @return Handle to the created sky texture
     */
    static tekki::render_graph::Handle<tekki::backend::vulkan::Image> RenderSkyCube(tekki::render_graph::RenderGraph& rg);

    /**
     * Convolve cube map
     * @param rg The render graph
     * @param input Input cube map texture
     * @return Handle to the convolved sky texture
     */
    static tekki::render_graph::Handle<tekki::backend::vulkan::Image> ConvolveCube(
        tekki::render_graph::RenderGraph& rg,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input
    );
};

} // namespace tekki::renderer::renderers