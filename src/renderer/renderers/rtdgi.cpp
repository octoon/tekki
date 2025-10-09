#include "../../include/tekki/renderer/renderers/rtdgi.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

constexpr vk::Format COLOR_BUFFER_FORMAT = vk::Format::eR16G16B16A16Sfloat;

RtdgiRenderer::RtdgiRenderer()
    : temporal_radiance_tex_("rtdgi.radiance"),
      temporal_ray_orig_tex_("rtdgi.ray_orig"),
      temporal_ray_tex_("rtdgi.ray"),
      temporal_reservoir_tex_("rtdgi.reservoir"),
      temporal_candidate_tex_("rtdgi.candidate"),
      temporal_invalidity_tex_("rtdgi.invalidity"),
      temporal2_tex_("rtdgi.temporal2"),
      temporal2_variance_tex_("rtdgi.temporal2_var"),
      temporal_hit_normal_tex_("rtdgi.hit_normal"),
      spatial_reuse_pass_count_(2),
      use_raytraced_reservoir_visibility_(false)
{
}

RtdgiOutput RtdgiRenderer::render(render_graph::TemporalRenderGraph& rg,
                                  const ReprojectedRtdgi& reprojected,
                                  const GbufferDepth& gbuffer_depth,
                                  const render_graph::Handle<vulkan::Image>& reprojection_map,
                                  const render_graph::Handle<vulkan::Image>& sky_cube,
                                  vk::DescriptorSet bindless_descriptor_set,
                                  const render_graph::Handle<vulkan::Image>& ssao_tex)
{
    const auto gbuffer_desc = rg.resource_desc(gbuffer_depth.gbuffer);
    const std::array<uint32_t, 2> half_extent = {gbuffer_desc.extent[0] / 2, gbuffer_desc.extent[1] / 2};

    // Extract half-resolution SSAO
    auto half_ssao_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR8Snorm),
        "half_ssao");

    rg.add_pass("extract_ssao_half",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({ssao_tex});
                    pb.writes({half_ssao_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement half-resolution SSAO extraction
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(half_ssao_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    // Create temporal resources for hit normals
    auto [hit_normal_output, hit_normal_history] = temporal_hit_normal_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR8G8B8A8Unorm));

    // Create temporal resources for candidates
    auto [candidate_output, candidate_history] = temporal_candidate_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR16G16B16A16Sfloat));

    // Create candidate textures
    auto candidate_radiance_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR16G16B16A16Sfloat),
        "candidate_radiance");

    auto candidate_normal_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR8G8B8A8Snorm),
        "candidate_normal");

    auto candidate_hit_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR16G16B16A16Sfloat),
        "candidate_hit");

    // Create temporal reservoir packed texture
    auto temporal_reservoir_packed_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR32G32B32A32Uint),
        "temporal_reservoir_packed");

    // Get half-resolution depth
    auto half_depth_tex = gbuffer_depth.half_depth(rg);

    // Create invalidity temporal resources
    auto [invalidity_output, invalidity_history] = temporal_invalidity_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR16G16Sfloat));

    // Main RTDGI ray tracing and reservoir sampling
    auto [radiance_tex, temporal_reservoir_tex] = render_ray_traced_diffuse(
        rg, gbuffer_depth, reprojected, reprojection_map, sky_cube, bindless_descriptor_set,
        candidate_radiance_tex, candidate_normal_tex, candidate_hit_tex,
        hit_normal_output, candidate_output, invalidity_output, invalidity_history);

    // Render irradiance with spatial resampling
    auto irradiance_tex = render_irradiance_with_resampling(
        rg, gbuffer_depth, radiance_tex, temporal_reservoir_tex, temporal_reservoir_packed_tex,
        candidate_radiance_tex, candidate_hit_tex, half_ssao_tex, reprojected.reprojected_history_tex,
        sky_cube, bindless_descriptor_set, half_extent, gbuffer_desc.extent);

    // Apply temporal filtering
    auto filtered_tex = apply_temporal_filtering(
        rg, irradiance_tex, gbuffer_depth, reprojection_map,
        reprojected.reprojected_history_tex, invalidity_output, reprojected.temporal_output_tex);

    // Apply spatial filtering
    auto final_tex = apply_spatial_filtering(
        rg, filtered_tex, gbuffer_depth, ssao_tex, bindless_descriptor_set);

    return RtdgiOutput{
        .screen_irradiance_tex = final_tex,
        .candidates = RtdgiCandidates{
            .candidate_radiance_tex = candidate_radiance_tex,
            .candidate_normal_tex = candidate_normal_tex,
            .candidate_hit_tex = candidate_hit_tex
        }
    };
}

ReprojectedRtdgi RtdgiRenderer::reproject(render_graph::TemporalRenderGraph& rg,
                                          const render_graph::Handle<vulkan::Image>& reprojection_map)
{
    const auto gbuffer_extent = rg.resource_desc(reprojection_map).extent;
    const std::array<uint32_t, 2> extent_2d = {gbuffer_extent[0], gbuffer_extent[1]};

    auto [temporal_output_tex, history_tex] = temporal2_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(extent_2d[0], extent_2d[1], COLOR_BUFFER_FORMAT));

    auto reprojected_history_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(extent_2d[0], extent_2d[1], COLOR_BUFFER_FORMAT),
        "reprojected_history");

    rg.add_pass("rtdgi_reproject",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({history_tex, reprojection_map});
                    pb.writes({reprojected_history_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement fullres reprojection compute shader
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(reprojected_history_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return ReprojectedRtdgi{
        .reprojected_history_tex = reprojected_history_tex,
        .temporal_output_tex = temporal_output_tex
    };
}

std::pair<render_graph::Handle<vulkan::Image>, render_graph::Handle<vulkan::Image>>
RtdgiRenderer::render_ray_traced_diffuse(
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
    const render_graph::Handle<vulkan::Image>& invalidity_history)
{
    const auto gbuffer_desc = rg.resource_desc(gbuffer_depth.gbuffer);
    const std::array<uint32_t, 2> half_extent = {gbuffer_desc.extent[0] / 2, gbuffer_desc.extent[1] / 2};

    auto [radiance_output, radiance_history] = temporal_radiance_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], COLOR_BUFFER_FORMAT));

    auto [ray_orig_output, ray_orig_history] = temporal_ray_orig_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR32G32B32A32Sfloat));

    auto [ray_output, ray_history] = temporal_ray_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR16G16B16A16Sfloat));

    auto [reservoir_output, reservoir_history] = temporal_reservoir_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR32G32Uint));

    auto half_view_normal_tex = gbuffer_depth.half_view_normal(rg);

    // RT validation pass
    auto rt_history_validity_pre_input_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR8Unorm),
        "rt_history_validity_pre");

    rg.add_pass("rtdgi_validate",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({half_view_normal_tex, gbuffer_depth.depth, reprojected.reprojected_history_tex,
                             ray_history, reprojection_map, sky_cube, ray_orig_history});
                    pb.writes({reservoir_history, radiance_history, rt_history_validity_pre_input_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement RTDGI validation ray tracing
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(rt_history_validity_pre_input_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    // RT trace pass
    auto rt_history_validity_input_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR8Unorm),
        "rt_history_validity");

    rg.add_pass("rtdgi_trace",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({half_view_normal_tex, gbuffer_depth.depth, reprojected.reprojected_history_tex,
                             reprojection_map, sky_cube, ray_orig_history, rt_history_validity_pre_input_tex});
                    pb.writes({candidate_radiance_tex, candidate_normal_tex, candidate_hit_tex, rt_history_validity_input_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement RTDGI trace ray tracing
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(candidate_radiance_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return {radiance_output, reservoir_output};
}

render_graph::Handle<vulkan::Image> RtdgiRenderer::render_irradiance_with_resampling(
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
    const std::array<uint32_t, 3>& full_extent)
{
    auto half_view_normal_tex = gbuffer_depth.half_view_normal(rg);
    auto half_depth_tex = gbuffer_depth.half_depth(rg);

    // Create ping-pong buffers for spatial resampling
    auto reservoir_output_tex0 = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR32G32Uint),
        "reservoir_spatial_0");

    auto reservoir_output_tex1 = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eR32G32Uint),
        "reservoir_spatial_1");

    auto bounced_radiance_output_tex0 = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eB10G11R11UfloatPack32),
        "bounced_radiance_0");

    auto bounced_radiance_output_tex1 = rg.create_image(
        vulkan::ImageDesc::new_2d(half_extent[0], half_extent[1], vk::Format::eB10G11R11UfloatPack32),
        "bounced_radiance_1");

    render_graph::Handle<vulkan::Image> reservoir_input_tex = temporal_reservoir_tex;
    render_graph::Handle<vulkan::Image> bounced_radiance_input_tex = radiance_tex;

    // Spatial reuse passes
    for (uint32_t spatial_pass = 0; spatial_pass < spatial_reuse_pass_count_; ++spatial_pass) {
        const bool is_final_pass = (spatial_pass + 1 == spatial_reuse_pass_count_);
        const uint32_t perform_occlusion_raymarch = is_final_pass ? 1 : 0;
        const uint32_t occlusion_raymarch_importance_only = use_raytraced_reservoir_visibility_ ? 1 : 0;

        rg.add_pass("restir_spatial",
                    [&](render_graph::PassBuilder& pb)
                    {
                        pb.reads({reservoir_input_tex, bounced_radiance_input_tex, half_view_normal_tex,
                                 half_depth_tex, gbuffer_depth.depth, half_ssao_tex, temporal_reservoir_packed_tex,
                                 reprojected_history_tex});
                        pb.writes({reservoir_output_tex0, bounced_radiance_output_tex0});
                        pb.executes([&](vk::CommandBuffer cmd) {
                            // TODO: Implement ReSTIR spatial resampling compute shader
                            vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0, .levelCount = 1,
                                .baseArrayLayer = 0, .layerCount = 1};
                            cmd.clearColorImage(pb.get_image(reservoir_output_tex0), vk::ImageLayout::eGeneral, &clear, 1, &range);
                        });
                    });

        // Swap ping-pong buffers
        std::swap(reservoir_output_tex0, reservoir_output_tex1);
        std::swap(bounced_radiance_output_tex0, bounced_radiance_output_tex1);
        reservoir_input_tex = reservoir_output_tex1;
        bounced_radiance_input_tex = bounced_radiance_output_tex1;
    }

    // Optional raytraced reservoir visibility check
    if (use_raytraced_reservoir_visibility_) {
        rg.add_pass("restir_check",
                    [&](render_graph::PassBuilder& pb)
                    {
                        pb.reads({half_depth_tex, temporal_reservoir_packed_tex});
                        pb.writes({reservoir_input_tex});
                        pb.executes([&](vk::CommandBuffer cmd) {
                            // TODO: Implement ReSTIR visibility check ray tracing
                        });
                    });
    }

    // Resolve final irradiance
    auto irradiance_output_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(full_extent[0], full_extent[1], COLOR_BUFFER_FORMAT),
        "irradiance_output");

    rg.add_pass("restir_resolve",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({radiance_tex, reservoir_input_tex, gbuffer_depth.gbuffer, gbuffer_depth.depth,
                             half_view_normal_tex, half_depth_tex, candidate_radiance_tex, candidate_hit_tex,
                             temporal_reservoir_packed_tex, bounced_radiance_input_tex});
                    pb.writes({irradiance_output_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement ReSTIR resolve compute shader
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(irradiance_output_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return irradiance_output_tex;
}

render_graph::Handle<vulkan::Image> RtdgiRenderer::apply_temporal_filtering(
    render_graph::TemporalRenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input_color,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& reprojection_map,
    const render_graph::Handle<vulkan::Image>& reprojected_history_tex,
    const render_graph::Handle<vulkan::Image>& rt_history_invalidity_tex,
    const render_graph::Handle<vulkan::Image>& temporal_output_tex)
{
    const auto input_desc = rg.resource_desc(input_color);

    auto [temporal_variance_output, variance_history] = temporal2_variance_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(input_desc.extent[0], input_desc.extent[1], vk::Format::eR16G16Sfloat));

    auto temporal_filtered_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(input_desc.extent[0], input_desc.extent[1], vk::Format::eR16G16B16A16Sfloat),
        "temporal_filtered");

    rg.add_pass("rtdgi_temporal",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input_color, reprojected_history_tex, variance_history, reprojection_map,
                             rt_history_invalidity_tex});
                    pb.writes({temporal_filtered_tex, temporal_output_tex, temporal_variance_output});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement temporal filtering compute shader
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(temporal_filtered_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return temporal_filtered_tex;
}

render_graph::Handle<vulkan::Image> RtdgiRenderer::apply_spatial_filtering(
    render_graph::TemporalRenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input_color,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& ssao_tex,
    vk::DescriptorSet bindless_descriptor_set)
{
    const auto input_desc = rg.resource_desc(input_color);

    auto spatial_filtered_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(input_desc.extent[0], input_desc.extent[1], COLOR_BUFFER_FORMAT),
        "spatial_filtered");

    rg.add_pass("rtdgi_spatial",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input_color, gbuffer_depth.depth, ssao_tex, gbuffer_depth.geometric_normal});
                    pb.writes({spatial_filtered_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement spatial filtering compute shader
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(spatial_filtered_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return spatial_filtered_tex;
}

} // namespace tekki::renderer