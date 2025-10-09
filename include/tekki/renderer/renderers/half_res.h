#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer::renderers {

class HalfRes {
public:
    /**
     * Extract half resolution GBuffer view normal as RGBA8
     * @param rg The render graph
     * @param gbuffer The GBuffer image handle
     * @return Half resolution view normal image
     */
    static std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> ExtractHalfResGbufferViewNormalRgba8(
        tekki::render_graph::RenderGraph& rg,
        const std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>>& gbuffer
    );

    /**
     * Extract half resolution depth
     * @param rg The render graph
     * @param depth The depth image handle
     * @return Half resolution depth image
     */
    static std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> ExtractHalfResDepth(
        tekki::render_graph::RenderGraph& rg,
        const std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>>& depth
    );
};

} // namespace tekki::renderer::renderers