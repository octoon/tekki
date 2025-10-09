#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

// Forward declarations for RTDGI types
struct ReprojectedRtdgi {
    render_graph::Handle<vulkan::Image> reprojected_history_tex;
    render_graph::Handle<vulkan::Image> temporal_output_tex;
};

struct RtdgiCandidates {
    render_graph::Handle<vulkan::Image> candidate_radiance_tex;
    render_graph::Handle<vulkan::Image> candidate_normal_tex;
    render_graph::Handle<vulkan::Image> candidate_hit_tex;
};

struct RtdgiOutput {
    render_graph::Handle<vulkan::Image> screen_irradiance_tex;
    RtdgiCandidates candidates;
};

class RtdgiRenderer {
public:
    RtdgiRenderer();

    RtdgiOutput render(
        render_graph::TemporalRenderGraph& rg,
        const ReprojectedRtdgi& reprojected,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::Image>& ssao_tex
    );

    ReprojectedRtdgi reproject(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& reprojection_map
    );

    // Configuration
    uint32_t spatial_reuse_pass_count_;
    bool use_raytraced_reservoir_visibility_;

private:
    // Temporal resources for RTDGI
    PingPongTemporalResource temporal_radiance_tex_;
    PingPongTemporalResource temporal_ray_orig_tex_;
    PingPongTemporalResource temporal_ray_tex_;
    PingPongTemporalResource temporal_reservoir_tex_;
    PingPongTemporalResource temporal_candidate_tex_;
    PingPongTemporalResource temporal_invalidity_tex_;
    PingPongTemporalResource temporal2_tex_;
    PingPongTemporalResource temporal2_variance_tex_;
    PingPongTemporalResource temporal_hit_normal_tex_;

    // Helper methods for RTDGI pipeline
    std::pair<render_graph::Handle<vulkan::Image>, render_graph::Handle<vulkan::Image>>
    render_ray_traced_diffuse(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const ReprojectedRtdgi& reprojected,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const render_graph::Handle<vulkan::Image>& candidate_radiance_tex,
        const render_graph::Handle<vulkan::Image>& candidate_normal_tex,
        const render_graph::Handle<vulkan::Image>& candidate_hit_tex,
        const render_graph::Handle<vulkan::Image>& hit_normal_output,
        const render_graph::Handle<vulkan::Image>& candidate_output,
        const render_graph::Handle<vulkan::Image>& invalidity_output,
        const render_graph::Handle<vulkan::Image>& invalidity_history
    );

    render_graph::Handle<vulkan::Image> render_irradiance_with_resampling(
        render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& radiance_tex,
        const render_graph::Handle<vulkan::Image>& temporal_reservoir_tex,
        const render_graph::Handle<vulkan::Image>& temporal_reservoir_packed_tex,
        const render_graph::Handle<vulkan::Image>& candidate_radiance_tex,
        const render_graph::Handle<vulkan::Image>& candidate_hit_tex,
        const render_graph::Handle<vulkan::Image>& half_ssao_tex,
        const render_graph::Handle<vulkan::Image>& reprojected_history_tex,
        const render_graph::Handle<vulkan::Image>& sky_cube,
        vk::DescriptorSet bindless_descriptor_set,
        const std::array<uint32_t, 2>& half_extent,
        const std::array<uint32_t, 3>& full_extent
    );

    render_graph::Handle<vulkan::Image> apply_temporal_filtering(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input_color,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& reprojected_history_tex,
        const render_graph::Handle<vulkan::Image>& rt_history_invalidity_tex,
        const render_graph::Handle<vulkan::Image>& temporal_output_tex
    );

    render_graph::Handle<vulkan::Image> apply_spatial_filtering(
        render_graph::TemporalRenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input_color,
        const GbufferDepth& gbuffer_depth,
        const render_graph::Handle<vulkan::Image>& ssao_tex,
        vk::DescriptorSet bindless_descriptor_set
    );
};

} // namespace tekki::renderer