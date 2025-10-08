#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <glm/glm.hpp>

namespace tekki::renderer {

struct TaaOutput {
    render_graph::ReadOnlyHandle<vulkan::Image> temporal_out;
    render_graph::Handle<vulkan::Image> this_frame_out;
};

class TaaRenderer {
public:
    TaaRenderer();

    TaaOutput render(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input_tex,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& depth_tex,
        const std::array<uint32_t, 2>& output_extent
    );

    glm::vec2 current_supersample_offset() const { return current_supersample_offset_; }

private:
    PingPongTemporalResource temporal_tex_;
    PingPongTemporalResource temporal_velocity_tex_;
    PingPongTemporalResource temporal_smooth_var_tex_;
    glm::vec2 current_supersample_offset_;

    static vulkan::ImageDesc temporal_tex_desc(const std::array<uint32_t, 2>& extent);
};

} // namespace tekki::renderer