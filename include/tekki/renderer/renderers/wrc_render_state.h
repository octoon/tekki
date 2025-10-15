#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"

namespace tekki::renderer::renderers {

struct IrcacheRenderState;  // Forward declaration - must match the actual definition

// World Radiance Cache render state
struct WrcRenderState {
    tekki::render_graph::Handle<tekki::backend::vulkan::Image> RadianceAtlas;

    WrcRenderState() = default;

    WrcRenderState(tekki::render_graph::Handle<tekki::backend::vulkan::Image> radianceAtlas)
        : RadianceAtlas(radianceAtlas) {}

    void SeeThrough(
        tekki::render_graph::TemporalRenderGraph& rg,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& skyCube,
        IrcacheRenderState& ircache,
        VkDescriptorSet bindlessDescriptorSet,
        const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
        tekki::render_graph::Handle<tekki::backend::vulkan::Image>& outputImg
    );
};

} // namespace tekki::renderer::renderers
