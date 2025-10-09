#include "../../../include/tekki/renderer/renderers/sky.h"
#include "../../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

render_graph::Handle<vulkan::Image> render_sky_cube(render_graph::RenderGraph& rg) {
    const uint32_t width = 64;

    // Create cube map texture
    auto sky_desc = vulkan::ImageDesc::new_cube(width, vk::Format::eR16G16B16A16Sfloat);
    auto sky_tex = rg.create(sky_desc);

    // Create compute pass
    auto pass = rg.add_pass("sky cube");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/sky/comp_cube.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register write with 2D array view (for cube map)
    auto view_desc = vulkan::ImageViewDesc::builder()
        .view_type(vk::ImageViewType::e2DArray)
        .build();

    auto sky_ref = pass.write_view(
        sky_tex,
        view_desc,
        vulkan::AccessType::ComputeShaderWrite
    );

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {render_graph::RenderPassBinding::Image(sky_ref)})
        );

        // Dispatch for all 6 cube faces
        api.cb().dispatch(width, width, 6);
    });

    return sky_tex;
}

render_graph::Handle<vulkan::Image> convolve_cube(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input
) {
    const uint32_t width = 16;

    // Create convolved cube map texture
    auto desc = vulkan::ImageDesc::new_cube(width, vk::Format::eR16G16B16A16Sfloat);
    auto sky_tex = rg.create(desc);

    // Create compute pass
    auto pass = rg.add_pass("convolve sky");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/convolve_cube.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register input read
    auto input_ref = pass.read(
        input,
        vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
    );

    // Register output write with 2D array view
    auto view_desc = vulkan::ImageViewDesc::builder()
        .view_type(vk::ImageViewType::e2DArray)
        .build();

    auto output_ref = pass.write_view(
        sky_tex,
        view_desc,
        vulkan::AccessType::ComputeShaderWrite
    );

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline with input and output
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {
                    render_graph::RenderPassBinding::Image(input_ref),
                    render_graph::RenderPassBinding::Image(output_ref)
                })
                .constants(&width, sizeof(width))
        );

        // Dispatch for all 6 cube faces
        api.cb().dispatch(width, width, 6);
    });

    return sky_tex;
}

} // namespace tekki::renderer
