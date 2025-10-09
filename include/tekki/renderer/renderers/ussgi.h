#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/renderer/render_graph/render_graph.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

class UssgiRenderer {
public:
    UssgiRenderer();
    
    std::shared_ptr<tekki::backend::vulkan::Image> Render(
        tekki::renderer::RenderGraph& renderGraph,
        const std::shared_ptr<GbufferDepth>& gbufferDepth,
        const std::shared_ptr<tekki::backend::vulkan::Image>& reprojectionMap,
        const std::shared_ptr<tekki::backend::vulkan::Image>& prevRadiance,
        VkDescriptorSet bindlessDescriptorSet
    );

private:
    static tekki::backend::vulkan::ImageDesc TemporalTexDesc(const glm::uvec2& extent);
    
    PingPongTemporalResource ussgiTex;
};

} // namespace tekki::renderer::renderers