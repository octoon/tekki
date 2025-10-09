#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/ray_tracing.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Reference path tracer for ground truth validation
void reference_path_trace(
    render_graph::RenderGraph& rg,
    render_graph::Handle<vulkan::Image>& output_img,
    vk::DescriptorSet bindless_descriptor_set,
    const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas
);

} // namespace tekki::renderer
