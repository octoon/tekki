#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"
#include "tekki/render_graph/lib.h"

namespace tekki::renderer::renderers {

struct TaaOutput {
    tekki::render_graph::ReadOnlyHandle<tekki::render_graph::Image> TemporalOut;
    tekki::render_graph::Handle<tekki::render_graph::Image> ThisFrameOut;
};

class TaaRenderer {
public:
    TaaRenderer();

    TaaOutput Render(
        tekki::render_graph::TemporalRenderGraph& rg,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& inputTex,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& depthTex,
        const std::array<uint32_t, 2>& outputExtent
    );

    glm::vec2 CurrentSupersampleOffset;

private:
    static tekki::render_graph::ImageDesc TemporalTexDesc(const std::array<uint32_t, 2>& extent);

    PingPongTemporalResource temporalTex;
    PingPongTemporalResource temporalVelocityTex;
    PingPongTemporalResource temporalSmoothVarTex;
};

}