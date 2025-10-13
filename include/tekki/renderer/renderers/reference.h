#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/render_graph/graph.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;
using RayTracingAcceleration = tekki::backend::vulkan::RayTracingAcceleration;

class ReferencePathTrace {
public:
    static void Execute(
        rg::RenderGraph& renderGraph,
        rg::Handle<Image>& outputImage,
        VkDescriptorSet bindlessDescriptorSet,
        const rg::Handle<RayTracingAcceleration>& tlas
    );
};

} // namespace tekki::renderer::renderers