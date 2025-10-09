#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../render_graph/Temporal.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/ray_tracing.h"
#include "ircache.h"

namespace tekki::renderer {

// World Radiance Cache (WRC) - Alternative global illumination technique
// Uses a sparse grid of radiance probes for efficient GI sampling
class WrcRenderer {
public:
    // WRC grid dimensions (must match shader constants)
    static constexpr std::array<uint32_t, 3> WRC_GRID_DIMS = {8, 3, 8};
    static constexpr uint32_t WRC_PROBE_DIMS = 32;
    static constexpr std::array<uint32_t, 2> WRC_ATLAS_PROBE_COUNT = {16, 16};

    // WRC render state containing radiance atlas
    struct WrcRenderState {
        render_graph::Handle<vulkan::Image> radiance_atlas;
    };

    // Trace WRC probes using ray tracing
    // Parameters:
    //   rg: Temporal render graph
    //   ircache: Irradiance cache for fallback GI
    //   sky_cube: Sky environment cube map
    //   bindless_descriptor_set: Bindless texture access
    //   tlas: Top-level acceleration structure for ray tracing
    // Returns: WRC render state with populated radiance atlas
    static WrcRenderState trace_wrc(
        render_graph::TemporalRenderGraph& rg,
        IrcacheRenderer::IrcacheRenderState& ircache,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas
    );

    // Allocate dummy WRC output for when WRC is disabled
    static WrcRenderState allocate_dummy_output(render_graph::TemporalRenderGraph& rg);

    // Apply WRC see-through rendering for transparency effects
    // Parameters:
    //   wrc_state: WRC render state with radiance atlas
    //   rg: Temporal render graph
    //   sky_cube: Sky environment cube map
    //   ircache: Irradiance cache for fallback
    //   bindless_descriptor_set: Bindless texture access
    //   tlas: Top-level acceleration structure
    //   output_img: Output image to render into
    static void render_see_through(
        const WrcRenderState& wrc_state,
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        IrcacheRenderer::IrcacheRenderState& ircache,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas,
        const render_graph::Handle<vulkan::Image>& output_img
    );

private:
    // Calculate total probe count for ray tracing dispatch
    static constexpr uint32_t calculate_total_probe_count() {
        return WRC_GRID_DIMS[0] * WRC_GRID_DIMS[1] * WRC_GRID_DIMS[2];
    }

    // Calculate total pixel count for atlas tracing
    static constexpr uint32_t calculate_total_pixel_count() {
        return calculate_total_probe_count() * WRC_PROBE_DIMS * WRC_PROBE_DIMS;
    }

    // Create radiance atlas image descriptor
    static vulkan::ImageDesc create_radiance_atlas_desc();
};

} // namespace tekki::renderer