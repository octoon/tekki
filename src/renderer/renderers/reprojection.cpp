#include "../../../include/tekki/renderer/renderers/reprojection.h"
#include "../../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

render_graph::Handle<vulkan::Image> calculate_reprojection_map(
    render_graph::TemporalRenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::Image>& velocity_img
) {
    // Create output reprojection map
    auto output_desc = gbuffer_depth.depth.desc().format(vk::Format::eR16G16B16A16Snorm);
    auto output_tex = rg.create(output_desc);

    // Get or create temporal previous depth buffer
    auto prev_depth_desc = gbuffer_depth.depth.desc()
        .format(vk::Format::eR32Sfloat)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);

    auto prev_depth = rg.get_or_create_temporal("reprojection.prev_depth", prev_depth_desc);

    // Create reprojection map computation pass
    {
        auto pass = rg.add_pass("reprojection map");

        // Register compute shader
        auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
            .hlsl_source("/shaders/calculate_reprojection_map.hlsl")
            .build();

        auto pipeline = pass.register_compute_pipeline({shader_desc});

        // Register reads
        auto depth_ref = pass.read_aspect(
            gbuffer_depth.depth,
            vk::ImageAspectFlagBits::eDepth,
            vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        auto geometric_normal_ref = pass.read(
            gbuffer_depth.geometric_normal,
            vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        auto prev_depth_ref = pass.read(
            prev_depth,
            vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        auto velocity_ref = pass.read(
            velocity_img,
            vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        // Register write
        auto output_ref = pass.write(
            output_tex,
            vulkan::AccessType::ComputeShaderWrite
        );

        // Get extent and constants
        auto extent = output_tex.desc().extent;
        auto extent_inv_extent = output_tex.desc().extent_inv_extent_2d();

        // Dispatch callback
        pass.dispatch([=](render_graph::PassApi& api) {
            // Bind pipeline
            auto bound_pipeline = api.bind_compute_pipeline(
                pipeline
                    .into_binding()
                    .descriptor_set(0, {
                        render_graph::RenderPassBinding::Image(depth_ref),
                        render_graph::RenderPassBinding::Image(geometric_normal_ref),
                        render_graph::RenderPassBinding::Image(prev_depth_ref),
                        render_graph::RenderPassBinding::Image(velocity_ref),
                        render_graph::RenderPassBinding::Image(output_ref)
                    })
                    .constants(&extent_inv_extent, sizeof(extent_inv_extent))
            );

            // Dispatch compute
            uint32_t group_size_x = (extent[0] + 7) / 8;
            uint32_t group_size_y = (extent[1] + 7) / 8;
            api.cb().dispatch(group_size_x, group_size_y, 1);
        });
    }

    // Copy current depth to previous depth for next frame
    {
        auto pass = rg.add_pass("copy depth");

        // Register compute shader (simple copy shader)
        auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
            .hlsl_source("/shaders/copy_depth_to_r.hlsl")  // or use Rust-generated shader
            .build();

        auto pipeline = pass.register_compute_pipeline({shader_desc});

        // Register read from current depth
        auto depth_ref = pass.read_aspect(
            gbuffer_depth.depth,
            vk::ImageAspectFlagBits::eDepth,
            vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
        );

        // Register write to previous depth
        auto prev_depth_ref = pass.write(
            prev_depth,
            vulkan::AccessType::ComputeShaderWrite
        );

        // Get extent
        auto extent = prev_depth.desc().extent;

        // Dispatch callback
        pass.dispatch([=](render_graph::PassApi& api) {
            // Bind pipeline
            auto bound_pipeline = api.bind_compute_pipeline(
                pipeline
                    .into_binding()
                    .descriptor_set(0, {
                        render_graph::RenderPassBinding::Image(depth_ref),
                        render_graph::RenderPassBinding::Image(prev_depth_ref)
                    })
            );

            // Dispatch compute
            uint32_t group_size_x = (extent[0] + 7) / 8;
            uint32_t group_size_y = (extent[1] + 7) / 8;
            api.cb().dispatch(group_size_x, group_size_y, 1);
        });
    }

    return output_tex;
}

} // namespace tekki::renderer
