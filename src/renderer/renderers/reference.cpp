#include "../../../include/tekki/renderer/renderers/reference.h"
#include "../../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

void reference_path_trace(
    render_graph::RenderGraph& rg,
    render_graph::Handle<vulkan::Image>& output_img,
    vk::DescriptorSet bindless_descriptor_set,
    const render_graph::Handle<vulkan::RayTracingAcceleration>& tlas
) {
    // Create ray tracing pass
    auto pass = rg.add_pass("reference pt");

    // Register ray tracing shaders
    auto rgen_shader = vulkan::ShaderSource::hlsl("/shaders/rt/reference_path_trace.rgen.hlsl");

    // Miss shaders
    std::vector<vulkan::ShaderSource> miss_shaders = {
        vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rmiss.hlsl"),
        vulkan::ShaderSource::hlsl("/shaders/rt/shadow.rmiss.hlsl")
    };

    // Hit group shaders
    std::vector<vulkan::RayTracingHitGroup> hit_groups = {
        vulkan::RayTracingHitGroup{
            vulkan::ShaderSource::hlsl("/shaders/rt/gbuffer.rchit.hlsl"),
            std::nullopt,  // No any-hit shader
            std::nullopt   // No intersection shader
        }
    };

    auto pipeline = pass.register_rt_pipeline(rgen_shader, miss_shaders, hit_groups);

    // Register write to output image
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
                    render_graph::RenderPassBinding::Image(output_ref),
                    render_graph::RenderPassBinding::AccelerationStructure(tlas_ref)
                })
                .raw_descriptor_set(1, bindless_descriptor_set)
        );

        // Trace rays
        bound_pipeline.trace_rays(api.cb(), extent[0], extent[1], extent[2]);
    });
}

} // namespace tekki::renderer
