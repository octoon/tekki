#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class LightingRenderer {
public:
    LightingRenderer();

    render_graph::Handle<vulkan::Image> render(
        render_graph::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& ssgi,
        const render_graph::Handle<vulkan::Image>& rtr,
        const render_graph::Handle<vulkan::Image>& ibl,
        const std::array<uint32_t, 2>& output_extent
    );

    render_graph::Handle<vulkan::Image> render_specular(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        vk::DescriptorSet bindless_descriptor_set,
        const std::array<uint32_t, 2>& output_extent
    );

private:
    // Lighting state
    std::shared_ptr<vulkan::Buffer> lighting_constants_buffer_;
};

} // namespace tekki::renderer