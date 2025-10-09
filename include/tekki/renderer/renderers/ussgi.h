#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../render_graph/Temporal.h"
#include "../../backend/vulkan/image.h"
#include "renderers.h"

namespace tekki::renderer {

// Ultra-wide Screen Space Global Illumination (USSGI) renderer
// An alternative SSGI implementation with wider sampling patterns
class UssgiRenderer {
public:
    UssgiRenderer();
    ~UssgiRenderer() = default;

    // Render USSGI effect
    // Parameters:
    //   rg: Temporal render graph for multi-frame resources
    //   gbuffer_depth: G-buffer and depth information
    //   reprojection_map: Motion vectors for temporal reuse
    //   prev_radiance: Previous frame radiance for GI
    //   bindless_descriptor_set: Bindless texture access
    // Returns: USSGI illuminated result
    render_graph::Handle<vulkan::Image> render(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& prev_radiance,
        vk::DescriptorSet bindless_descriptor_set
    );

private:
    // Temporal resource for ping-pong USSGI textures
    PingPongTemporalResource ussgi_tex_;

    // Texture format for USSGI computations
    static constexpr vk::Format TEX_FMT = vk::Format::eR16G16B16A16Sfloat;

    // Create temporal texture descriptor
    static vulkan::ImageDesc temporal_tex_desc(const std::array<uint32_t, 2>& extent);

    // Compute USSGI from G-buffer and previous radiance
    render_graph::Handle<vulkan::Image> compute_ussgi(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& prev_radiance,
        vk::DescriptorSet bindless_descriptor_set
    );

    // Apply temporal filtering to USSGI result
    render_graph::Handle<vulkan::Image> apply_temporal_filter(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& ussgi_tex,
        const render_graph::Handle<vulkan::Image>& reprojection_map
    );
};

} // namespace tekki::renderer