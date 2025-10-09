#include "../../../include/tekki/renderer/renderers/deferred.h"
#include "../../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

void light_gbuffer(
    render_graph::RenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& shadow_mask,
    const render_graph::Handle<vulkan::Image>& rtr,
    const render_graph::Handle<vulkan::Image>& rtdgi,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    render_graph::Handle<vulkan::Image>& temporal_output,
    render_graph::Handle<vulkan::Image>& output,
    const render_graph::Handle<vulkan::Image>& sky_cube,
    const render_graph::Handle<vulkan::Image>& convolved_sky_cube,
    vk::DescriptorSet bindless_descriptor_set,
    size_t debug_shading_mode,
    bool debug_show_wrc
) {
    // Create compute pass
    auto pass = rg.add_pass("light gbuffer");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/light_gbuffer.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register resource reads
    auto gbuffer_ref = pass.read(gbuffer_depth.gbuffer, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);

    auto depth_ref = pass.read_aspect(
        gbuffer_depth.depth,
        vk::ImageAspectFlagBits::eDepth,
        vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
    );

    auto shadow_mask_ref = pass.read(shadow_mask, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
    auto rtr_ref = pass.read(rtr, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
    auto rtdgi_ref = pass.read(rtdgi, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);

    // Bind ircache and wrc
    auto ircache_ref = pass.bind_mut(ircache);
    auto wrc_ref = pass.bind(wrc);

    // Register writes
    auto temporal_output_ref = pass.write(temporal_output, vulkan::AccessType::ComputeShaderWrite);
    auto output_ref = pass.write(output, vulkan::AccessType::ComputeShaderWrite);

    // Register sky cube reads
    auto sky_cube_ref = pass.read(sky_cube, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
    auto convolved_sky_cube_ref = pass.read(convolved_sky_cube, vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);

    // Get extent from gbuffer
    auto extent = gbuffer_depth.gbuffer.desc().extent;
    auto extent_inv_extent = gbuffer_depth.gbuffer.desc().extent_inv_extent_2d();

    // Prepare constants
    struct Constants {
        std::array<float, 4> extent_inv_extent;
        uint32_t debug_shading_mode;
        uint32_t debug_show_wrc;
    } constants = {
        extent_inv_extent,
        static_cast<uint32_t>(debug_shading_mode),
        static_cast<uint32_t>(debug_show_wrc)
    };

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline with descriptor sets
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {
                    render_graph::RenderPassBinding::Image(gbuffer_ref),
                    render_graph::RenderPassBinding::Image(depth_ref),
                    render_graph::RenderPassBinding::Image(shadow_mask_ref),
                    render_graph::RenderPassBinding::Image(rtr_ref),
                    render_graph::RenderPassBinding::Image(rtdgi_ref),
                    render_graph::RenderPassBinding::State(ircache_ref),
                    render_graph::RenderPassBinding::State(wrc_ref),
                    render_graph::RenderPassBinding::Image(temporal_output_ref),
                    render_graph::RenderPassBinding::Image(output_ref),
                    render_graph::RenderPassBinding::Image(sky_cube_ref),
                    render_graph::RenderPassBinding::Image(convolved_sky_cube_ref)
                })
                .raw_descriptor_set(1, bindless_descriptor_set)
                .constants(&constants, sizeof(constants))
        );

        // Dispatch compute shader
        uint32_t group_size_x = (extent[0] + 7) / 8;
        uint32_t group_size_y = (extent[1] + 7) / 8;
        api.cb().dispatch(group_size_x, group_size_y, 1);
    });
}

} // namespace tekki::renderer
