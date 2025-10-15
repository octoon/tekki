#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/lib.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

class UssgiRenderer {
public:
    UssgiRenderer();

    rg::Handle<Image> Render(
        rg::TemporalRenderGraph& renderGraph,
        const std::shared_ptr<GbufferDepth>& gbufferDepth,
        const rg::Handle<Image>& reprojectionMap,
        const rg::Handle<Image>& prevRadiance,
        VkDescriptorSet bindlessDescriptorSet
    );

private:
    static rg::ImageDesc TemporalTexDesc(const glm::uvec2& extent);

    PingPongTemporalResource ussgiTex_;
};

} // namespace tekki::renderer::renderers