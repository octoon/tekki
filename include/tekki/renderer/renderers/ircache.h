#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class IrcacheRenderer {
public:
    IrcacheRenderer();

    render_graph::ReadOnlyHandle<vulkan::Image> render(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& prev_radiance,
        vk::DescriptorSet bindless_descriptor_set
    );

    void update_eye_position(const glm::vec3& eye_position);
    const std::vector<IrcacheCascadeConstants>& constants() const;
    glm::vec3 grid_center() const;

private:
    PingPongTemporalResource ircache_tex_;

    void filter_ircache(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        PingPongTemporalResource& temporal_tex
    );

    // IRCache state
    std::shared_ptr<vulkan::Buffer> ircache_constants_buffer_;
    std::vector<IrcacheCascadeConstants> ircache_cascades_;
    glm::vec3 eye_position_;
    glm::vec3 grid_center_;
};

} // namespace tekki::renderer