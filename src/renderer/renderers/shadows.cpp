#include "../../../include/tekki/renderer/renderers/shadows.h"
#include "../../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

render_graph::Handle<vulkan::Image> trace_sun_shadow_mask(
    render_graph::RenderGraph& rg,
    const GbufferDepth& gbuffer_depth,
    const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas,
    vk::DescriptorSet bindless_descriptor_set
) {
    // Create output image with same resolution as depth buffer
    auto output_desc = gbuffer_depth.depth.desc().format(vk::Format::eR8Unorm);
    auto output_img = rg.create(output_desc);

    // Create ray tracing pass
    auto pass = rg.add_pass("trace shadow mask");

    // Register ray tracing shaders
    auto rgen_shader = vulkan::ShaderSource::hlsl("/shaders/rt/trace_sun_shadow_mask.rgen.hlsl");

    // Miss shaders (duplicated because rt.hlsl hardcodes miss index to 1)
    std::vector<vulkan::ShaderSource> miss_shaders = {
        vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl"),
        vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
    };

    // No hit groups for shadow rays (empty)
    std::vector<vulkan::RayTracingHitGroup> hit_groups = {};

    auto pipeline = pass.register_rt_pipeline(rgen_shader, miss_shaders, hit_groups);

    // Register reads
    auto depth_ref = pass.read_aspect(
        gbuffer_depth.depth,
        vk::ImageAspectFlagBits::eDepth,
        vulkan::AccessType::RayTracingShaderReadSampledImageOrUniformTexelBuffer
    );

    auto geometric_normal_ref = pass.read(
        gbuffer_depth.geometric_normal,
        vulkan::AccessType::RayTracingShaderReadSampledImageOrUniformTexelBuffer
    );

    // Register write
    auto output_ref = pass.write(
        output_img,
        vulkan::AccessType::RayTracingShaderWrite
    );

    // Register TLAS read
    auto tlas_ref = pass.read(
        tlas,
        vulkan::AccessType::RayTracingShaderReadAccelerationStructure
    );

    // Get extent
    auto extent = output_img.desc().extent;

    // Trace rays callback
    pass.trace_rays([=](render_graph::PassApi& api) {
        // Bind pipeline with descriptor sets
        auto bound_pipeline = api.bind_rt_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {
                    render_graph::RenderPassBinding::Image(depth_ref),
                    render_graph::RenderPassBinding::Image(geometric_normal_ref),
                    render_graph::RenderPassBinding::Image(output_ref),
                    render_graph::RenderPassBinding::AccelerationStructure(tlas_ref)
                })
                .raw_descriptor_set(1, bindless_descriptor_set)
        );

        // Trace rays
        bound_pipeline.trace_rays(api.cb(), extent[0], extent[1], extent[2]);
    });

    return output_img;
}

} // namespace tekki::renderer
