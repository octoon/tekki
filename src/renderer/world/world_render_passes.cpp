#include "../../../include/tekki/renderer/world/WorldRenderer.h"
#include "../../../include/tekki/renderer/renderers/raster_meshes.h"
#include "../../../include/tekki/renderer/renderers/deferred.h"
#include "../../../include/tekki/renderer/renderers/reprojection.h"
#include "../../../include/tekki/renderer/renderers/sky.h"
#include "../../../include/tekki/renderer/renderers/shadows.h"
#include "../../../include/tekki/renderer/renderers/reference.h"
#include "../../../include/tekki/renderer/renderers/motion_blur.h"
#include "../../../include/tekki/core/log.h"

namespace tekki::renderer::world {

render_graph::Handle<vulkan::RayTracingAcceleration> WorldRenderer::prepare_top_level_acceleration(
    render_graph::RenderGraph& rg)
{
    // Build acceleration structures if not already done
    build_ray_tracing_top_level_acceleration();

    // Import TLAS into render graph
    if (tlas_) {
        return rg.import_acceleration_structure(tlas_, "tlas");
    }

    // Return dummy handle if no TLAS
    throw std::runtime_error("Ray tracing not enabled or TLAS not built");
}

const ExposureState& WorldRenderer::exposure_state() const
{
    return exposure_state_[static_cast<size_t>(render_mode_)];
}

render_graph::Handle<vulkan::Image> WorldRenderer::prepare_render_graph_standard(
    render_graph::TemporalRenderGraph& rg,
    const WorldFrameDesc& frame_desc)
{
    TEKKI_LOG_DEBUG("Preparing standard render graph");

    // Prepare TLAS if ray tracing is enabled
    std::optional<render_graph::Handle<vulkan::RayTracingAcceleration>> tlas;
    if (device_->ray_tracing_enabled()) {
        tlas = prepare_top_level_acceleration(rg);
    }

    // Create temporal accumulation image
    auto accum_img = rg.get_or_create_temporal(
        "root.accum",
        vulkan::ImageDesc::new_2d(
            frame_desc.render_extent[0],
            frame_desc.render_extent[1],
            vk::Format::eR16G16B16A16Sfloat
        ).usage(vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eStorage |
                vk::ImageUsageFlagBits::eTransferDst)
    );

    // Render or import sky cube
    auto sky_cube = ibl_.render(rg).value_or(
        render_sky_cube(rg)
    );

    auto convolved_sky_cube = convolve_sky_cube(rg, sky_cube);

    // Create G-buffer and velocity
    GbufferDepth gbuffer_depth;
    render_graph::Handle<vulkan::Image> velocity_img;

    {
        auto normal = rg.create_image(
            vulkan::ImageDesc::new_2d(
                frame_desc.render_extent[0],
                frame_desc.render_extent[1],
                vk::Format::eA2R10G10B10UnormPack32
            ),
            "gbuffer_normal"
        );

        auto gbuffer = rg.create_image(
            vulkan::ImageDesc::new_2d(
                frame_desc.render_extent[0],
                frame_desc.render_extent[1],
                vk::Format::eR32G32B32A32Sfloat
            ),
            "gbuffer"
        );

        auto depth_img = rg.create_image(
            vulkan::ImageDesc::new_2d(
                frame_desc.render_extent[0],
                frame_desc.render_extent[1],
                vk::Format::eD32Sfloat
            ),
            "depth"
        );

        // Clear depth
        rg.clear_depth(depth_img);

        gbuffer_depth = GbufferDepth(normal, gbuffer, depth_img);

        velocity_img = rg.create_image(
            vulkan::ImageDesc::new_2d(
                frame_desc.render_extent[0],
                frame_desc.render_extent[1],
                vk::Format::eR16G16B16A16Sfloat
            ),
            "velocity"
        );

        // Rasterize meshes
        raster_meshes(
            rg,
            raster_simple_render_pass_,
            gbuffer_depth,
            velocity_img,
            RasterMeshesData{
                .meshes = meshes_,
                .instances = instances_,
                .vertex_buffer = vertex_buffer_,
                .bindless_descriptor_set = bindless_descriptor_set_
            }
        );
    }

    // Calculate reprojection map
    auto reprojection_map = calculate_reprojection_map(rg, gbuffer_depth, velocity_img);

    // SSGI
    auto ssgi_tex = ssgi_.render(rg, gbuffer_depth, reprojection_map, accum_img, bindless_descriptor_set_);

    // Prepare irradiance cache
    auto ircache_state = ircache_.prepare(rg);

    // WRC (World Radiance Cache) - currently disabled, allocate dummy
    auto wrc = WrcRenderer::allocate_dummy_output(rg);

    // Trace irradiance cache
    std::optional<IrcacheRenderer::TracedIrcache> traced_ircache;
    if (tlas.has_value()) {
        traced_ircache = ircache_state.trace_irradiance(
            rg,
            convolved_sky_cube,
            bindless_descriptor_set_,
            tlas.value(),
            wrc
        );
    }

    // Trace sun shadow mask
    render_graph::Handle<vulkan::Image> sun_shadow_mask;
    if (tlas.has_value()) {
        sun_shadow_mask = trace_sun_shadow_mask(rg, gbuffer_depth, tlas.value(), bindless_descriptor_set_);
    } else {
        sun_shadow_mask = rg.create_image(
            vulkan::ImageDesc::new_2d(
                frame_desc.render_extent[0],
                frame_desc.render_extent[1],
                vk::Format::eR8Unorm
            ),
            "dummy_shadow_mask"
        );
    }

    // Reproject RTDGI
    auto reprojected_rtdgi = rtdgi_.reproject(rg, reprojection_map);

    // Denoise shadow mask
    auto denoised_shadow_mask = (sun_size_multiplier_ > 0.0f)
        ? shadow_denoise_.render(rg, gbuffer_depth, sun_shadow_mask, reprojection_map)
        : sun_shadow_mask;

    // Sum up irradiance for sampling
    if (traced_ircache.has_value()) {
        ircache_state.sum_up_irradiance_for_sampling(rg, traced_ircache.value());
    }

    // RTDGI rendering
    std::optional<render_graph::Handle<vulkan::Image>> rtdgi_irradiance;
    std::optional<render_graph::Handle<vulkan::Image>> rtdgi_candidates;

    if (tlas.has_value()) {
        auto rtdgi = rtdgi_.render(
            rg,
            reprojected_rtdgi,
            gbuffer_depth,
            reprojection_map,
            convolved_sky_cube,
            bindless_descriptor_set_,
            ircache_state,
            wrc,
            tlas.value(),
            ssgi_tex
        );
        rtdgi_irradiance = rtdgi.screen_irradiance_tex;
        rtdgi_candidates = rtdgi.candidates;
    }

    // Check for triangle lights
    bool any_triangle_lights = false;
    for (const auto& inst : instances_) {
        if (!mesh_lights_[inst.mesh.id].lights.empty()) {
            any_triangle_lights = true;
            break;
        }
    }

    // RTR (Ray-traced reflections)
    RtrRenderer::RtrOutput rtr;
    if (tlas.has_value() && rtdgi_irradiance.has_value() && rtdgi_candidates.has_value()) {
        rtr = rtr_.trace(
            rg,
            gbuffer_depth,
            reprojection_map,
            sky_cube,
            bindless_descriptor_set_,
            tlas.value(),
            rtdgi_irradiance.value(),
            rtdgi_candidates.value(),
            ircache_state,
            wrc
        );
    } else {
        rtr = rtr_.create_dummy_output(rg, gbuffer_depth);
    }

    // Render specular lighting into RTR
    if (any_triangle_lights && tlas.has_value()) {
        lighting_.render_specular(
            rtr.resolved_tex,
            rg,
            gbuffer_depth,
            bindless_descriptor_set_,
            tlas.value()
        );
    }

    // Filter RTR temporally
    auto rtr_filtered = rtr.filter_temporal(rg, gbuffer_depth, reprojection_map);

    // Create debug output texture
    auto debug_out_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(
            frame_desc.render_extent[0],
            frame_desc.render_extent[1],
            vk::Format::eR16G16B16A16Sfloat
        ),
        "debug_out"
    );

    // Get RTDGI or create dummy
    auto rtdgi_final = rtdgi_irradiance.value_or(
        rg.create_image(
            vulkan::ImageDesc::new_2d(1, 1, vk::Format::eR8G8B8A8Unorm),
            "dummy_rtdgi"
        )
    );

    // Light G-buffer
    light_gbuffer(
        rg,
        gbuffer_depth,
        denoised_shadow_mask,
        rtr_filtered,
        rtdgi_final,
        ircache_state,
        wrc,
        accum_img,
        debug_out_tex,
        sky_cube,
        convolved_sky_cube,
        bindless_descriptor_set_,
        debug_shading_mode_,
        debug_show_wrc_
    );

    // TAA (Temporal Anti-Aliasing)
    auto anti_aliased = taa_.render(
        rg,
        debug_out_tex,
        reprojection_map,
        gbuffer_depth.depth,
        temporal_upscale_extent_
    ).this_frame_out;

    // Motion blur
    auto final_post_input = motion_blur(rg, anti_aliased, gbuffer_depth.depth, reprojection_map);

    // WRC see-through (if debug mode enabled)
    if (tlas.has_value() && debug_mode_ == RenderDebugMode::WorldRadianceCache) {
        WrcRenderer::render_see_through(
            wrc,
            rg,
            convolved_sky_cube,
            ircache_state,
            bindless_descriptor_set_,
            tlas.value(),
            final_post_input
        );
    }

    // Post-processing
    auto post_processed = post_.render(
        rg,
        final_post_input,
        bindless_descriptor_set_,
        exposure_state().post_mult,
        contrast_,
        dynamic_exposure_.histogram_clipping
    );

    TEKKI_LOG_DEBUG("Standard render graph prepared");
    return post_processed;
}

render_graph::Handle<vulkan::Image> WorldRenderer::prepare_render_graph_reference(
    render_graph::TemporalRenderGraph& rg,
    const WorldFrameDesc& frame_desc)
{
    TEKKI_LOG_DEBUG("Preparing reference render graph (path tracer)");

    // Create temporal accumulation image
    auto accum_img = rg.get_or_create_temporal(
        "refpt.accum",
        vulkan::ImageDesc::new_2d(
            frame_desc.render_extent[0],
            frame_desc.render_extent[1],
            vk::Format::eR32G32B32A32Sfloat
        ).usage(vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eStorage |
                vk::ImageUsageFlagBits::eTransferDst)
    );

    // Reset accumulation if requested
    if (reset_reference_accumulation_) {
        reset_reference_accumulation_ = false;
        rg.clear_color(accum_img, std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f});
    }

    // Reference path tracing
    if (device_->ray_tracing_enabled()) {
        auto tlas = prepare_top_level_acceleration(rg);
        reference_path_trace(rg, accum_img, bindless_descriptor_set_, tlas);
    }

    // Post-processing
    auto post_processed = post_.render(
        rg,
        accum_img,
        bindless_descriptor_set_,
        exposure_state().post_mult,
        contrast_,
        dynamic_exposure_.histogram_clipping
    );

    TEKKI_LOG_DEBUG("Reference render graph prepared");
    return post_processed;
}

} // namespace tekki::renderer::world