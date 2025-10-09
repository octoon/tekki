#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/rg/simple_render_pass.h"

namespace tekki::renderer::renderers {

struct TaaOutput {
    rg::ReadOnlyHandle<Image> TemporalOut;
    rg::Handle<Image> ThisFrameOut;
};

class TaaRenderer {
public:
    TaaRenderer();
    
    TaaOutput Render(
        rg::TemporalRenderGraph& rg,
        const rg::Handle<Image>& inputTex,
        const rg::Handle<Image>& reprojectionMap,
        const rg::Handle<Image>& depthTex,
        const std::array<uint32_t, 2>& outputExtent
    );

    glm::vec2 CurrentSupersampleOffset;

private:
    static ImageDesc TemporalTexDesc(const std::array<uint32_t, 2>& extent);
    
    PingPongTemporalResource temporalTex;
    PingPongTemporalResource temporalVelocityTex;
    PingPongTemporalResource temporalSmoothVarTex;
};

}