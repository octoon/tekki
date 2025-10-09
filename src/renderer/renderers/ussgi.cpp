#include "../../../include/tekki/renderer/renderers/ussgi.h"
#include "../../../include/tekki/backend/vulkan/shader.h"
#include "../../../include/tekki/core/log.h"

namespace tekki::renderer {

UssgiRenderer::UssgiRenderer()
    : ussgi_tex_("ussgi")
{
}

render_graph::Handle<vulkan::Image> UssgiRenderer::render(
    render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& reprojection_map,
    const render_graph::Handle<vulkan::Image>& prev_radiance,
    vk::DescriptorSet bindless_descriptor_set)
{
    TEKKI_LOG_DEBUG("Starting USSGI rendering");

    // Compute USSGI from G-buffer
    auto ussgi_tex = compute_ussgi(rg, gbuffer_depth, reprojection_map, prev_radiance, bindless_descriptor_set);

    // Apply temporal filtering
    auto filtered_output = apply_temporal_filter(rg, ussgi_tex, reprojection_map);

    TEKKI_LOG_DEBUG("USSGI rendering completed");
    return filtered_output;
}

vulkan::ImageDesc UssgiRenderer::temporal_tex_desc(const std::array<uint32_t, 2>& extent)
{
    return vulkan::ImageDesc::new_2d(
        extent[0],
        extent[1],
        TEX_FMT
    ).usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
}

render_graph::Handle<vulkan::Image> UssgiRenderer::compute_ussgi(
    render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& reprojection_map,
    const render_graph::Handle<vulkan::Image>& prev_radiance,
    vk::DescriptorSet bindless_descriptor_set)
{
    const auto gbuffer_desc = rg.resource_desc(gbuffer_depth.gbuffer);
    auto half_view_normal_tex = gbuffer_depth.get_half_view_normal(rg);

    // Create USSGI computation texture
    auto ussgi_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(
            gbuffer_desc.extent[0],
            gbuffer_desc.extent[1],
            TEX_FMT
        ),
        "ussgi"
    );

    // USSGI computation pass
    rg.add_pass("ussgi",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({gbuffer_depth.gbuffer, gbuffer_depth.depth, half_view_normal_tex, prev_radiance, reprojection_map});
                    pb.writes({ussgi_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement USSGI compute shader
                        // This would involve:
                        // 1. Reading G-buffer (normals, materials, depth)
                        // 2. Reading half-resolution view normals for efficiency
                        // 3. Reading previous frame radiance for GI
                        // 4. Performing wide-pattern screen-space sampling
                        // 5. Computing indirect illumination contribution
                        // 6. Writing USSGI result to R16G16B16A16_SFLOAT texture

                        const auto output_desc = rg.resource_desc(ussgi_tex);
                        const uint32_t dispatch_x = (output_desc.extent[0] + 7) / 8;
                        const uint32_t dispatch_y = (output_desc.extent[1] + 7) / 8;

                        // TODO: Set bindless descriptor set and constants
                        // Constants would include:
                        // - gbuffer_desc.extent_inv_extent_2d()
                        // - ussgi_tex.desc().extent_inv_extent_2d()

                        TEKKI_LOG_DEBUG("USSGI compute: dispatching {}x{} work groups", dispatch_x, dispatch_y);
                        // cmd.dispatch(dispatch_x, dispatch_y, 1);
                    });
                });

    return ussgi_tex;
}

render_graph::Handle<vulkan::Image> UssgiRenderer::apply_temporal_filter(
    render_graph::TemporalRenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& ussgi_tex,
    const render_graph::Handle<vulkan::Image>& reprojection_map)
{
    const auto ussgi_desc = rg.resource_desc(ussgi_tex);
    const std::array<uint32_t, 2> extent = {ussgi_desc.extent[0], ussgi_desc.extent[1]};

    // Get temporal resources for ping-pong rendering
    auto [filtered_output_tex, history_tex] = ussgi_tex_.get_output_and_history(
        rg, temporal_tex_desc(extent)
    );

    // Temporal filtering pass
    rg.add_pass("ussgi_temporal",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({ussgi_tex, history_tex, reprojection_map});
                    pb.writes({filtered_output_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement USSGI temporal filtering compute shader
                        // This would involve:
                        // 1. Reading current frame USSGI result
                        // 2. Reading previous frame filtered result (history)
                        // 3. Reading reprojection map for temporal coherence
                        // 4. Blending current and previous results based on motion
                        // 5. Applying noise reduction and stability improvements

                        const auto output_desc = rg.resource_desc(filtered_output_tex);
                        const uint32_t dispatch_x = (output_desc.extent[0] + 7) / 8;
                        const uint32_t dispatch_y = (output_desc.extent[1] + 7) / 8;

                        // TODO: Pass constants for extent_inv_extent_2d()

                        TEKKI_LOG_DEBUG("USSGI temporal filter: dispatching {}x{} work groups", dispatch_x, dispatch_y);
                        // cmd.dispatch(dispatch_x, dispatch_y, 1);
                    });
                });

    return filtered_output_tex;
}

} // namespace tekki::renderer