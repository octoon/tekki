#pragma once

#include <string>
#include <utility>
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/Image.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

/**
 * Ping-pong temporal resource for swapping between output and history
 * Ported from kajiya's PingPongTemporalResource struct
 */
class PingPongTemporalResource {
private:
    rg::TemporalResourceKey output_tex_;
    rg::TemporalResourceKey history_tex_;

public:
    PingPongTemporalResource(const std::string& name)
        : output_tex_(name + ":0"),
          history_tex_(name + ":1") {}

    /**
     * Get output and history textures, automatically swapping them
     * Returns (output_tex, history_tex)
     */
    std::pair<rg::Handle<rg::Image>, rg::Handle<rg::Image>> GetOutputAndHistory(
        rg::TemporalRenderGraph& rg_,
        const rg::ImageDesc& desc) {

        auto output_tex = rg_.GetOrCreateTemporal(output_tex_, desc);
        auto history_tex = rg_.GetOrCreateTemporal(history_tex_, desc);

        // Swap for next frame
        std::swap(output_tex_, history_tex_);

        return std::make_pair(output_tex, history_tex);
    }
};

} // namespace tekki::renderer::renderers
