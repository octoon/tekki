#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>
#include <tuple>

namespace tekki::renderer {

// Forward declaration from rtdgi.h
struct RtdgiCandidates;

// RTR output structure
struct TracedRtr {
    render_graph::Handle<vulkan::Image> resolved_tex;
    render_graph::Handle<vulkan::Image> temporal_output_tex;
    render_graph::Handle<vulkan::Image> history_tex;
    render_graph::Handle<vulkan::Image> ray_len_tex;
    render_graph::Handle<vulkan::Image> refl_restir_invalidity_tex;
};

class RtrRenderer {
public:
    RtrRenderer();

    TracedRtr trace(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::Image>& rtdgi_irradiance,
        const RtdgiCandidates& rtdgi_candidates
    );

    TracedRtr create_dummy_output(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth
    );

    render_graph::Handle<vulkan::Image> temporal_filter(
        render_graph::TemporalRenderGraph& rg,
        const TracedRtr& traced_rtr,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map
    );

    // Configuration
    bool reuse_rtdgi_rays_;

private:
    // Temporal resources for RTR
    PingPongTemporalResource temporal_tex_;
    PingPongTemporalResource ray_len_tex_;
    PingPongTemporalResource temporal_irradiance_tex_;
    PingPongTemporalResource temporal_ray_orig_tex_;
    PingPongTemporalResource temporal_ray_tex_;
    PingPongTemporalResource temporal_reservoir_tex_;
    PingPongTemporalResource temporal_rng_tex_;
    PingPongTemporalResource temporal_hit_normal_tex_;

    // Helper method for ReSTIR temporal processing
    std::tuple<render_graph::Handle<vulkan::Image>, render_graph::Handle<vulkan::Image>,
               render_graph::Handle<vulkan::Image>, render_graph::Handle<vulkan::Image>>
    process_restir_temporal(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const std::array<uint32_t, 2>& half_extent,
        const render_graph::Handle<vulkan::Image>& rtdgi_irradiance,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::Image>& refl0_tex,
        const render_graph::Handle<vulkan::Image>& refl1_tex,
        const render_graph::Handle<vulkan::Image>& refl2_tex,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& ray_orig_history_tex,
        const render_graph::Handle<vulkan::Image>& rng_history_tex,
        const render_graph::Handle<vulkan::Image>& refl_restir_invalidity_tex
    );
};

} // namespace tekki::renderer