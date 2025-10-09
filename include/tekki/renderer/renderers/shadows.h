#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/ray_tracing.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Trace sun shadow mask using ray tracing
render_graph::Handle<vulkan::Image> trace_sun_shadow_mask(
    render_graph::RenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas,
    vk::DescriptorSet bindless_descriptor_set
);

} // namespace tekki::renderer