#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/renderer/renderers/wrc_render_state.h"

namespace tekki::renderer::renderers {

// Must match `wrc_settings.hlsl`
constexpr uint32_t WrcGridDims[3] = {8, 3, 8};
constexpr uint32_t WrcProbeDims = 32;
constexpr uint32_t WrcAtlasProbeCount[2] = {16, 16};

struct IrcacheRenderState;  // Forward declaration - must match the actual definition

class WrcRenderer {
public:
    static WrcRenderState WrcTrace(
        tekki::render_graph::TemporalRenderGraph& rg,
        IrcacheRenderState& ircache,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& skyCube,
        VkDescriptorSet bindlessDescriptorSet,
        const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas
    );

    static WrcRenderState AllocateDummyOutput(tekki::render_graph::TemporalRenderGraph& rg);
};

} // namespace tekki::renderer::renderers