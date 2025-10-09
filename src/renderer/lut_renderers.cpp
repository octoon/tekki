#include "../../include/tekki/renderer/lut_renderers.h"
#include "../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

// BRDF Fg LUT Computer Implementation

std::shared_ptr<vulkan::Image> BrdfFgLutComputer::create(vulkan::Device* device) {
    auto desc = vulkan::ImageDesc::new_2d(64, 64, vk::Format::eR16G16B16A16Sfloat)
        .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);

    return device->create_image(desc, "brdf_fg_lut");
}

void BrdfFgLutComputer::compute(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& img
) {
    auto pass = rg.add_pass("brdf_fg lut");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/lut/brdf_fg.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register write to output image
    auto img_ref = pass.write(img, vulkan::AccessType::ComputeShaderWrite);

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline with output image
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {render_graph::RenderPassBinding::Image(img_ref)})
        );

        // Dispatch compute shader - extent is already in groups for some shaders,
        // but for LUTs we typically want one thread per pixel
        auto extent = img_ref.desc().extent;
        uint32_t group_size_x = (extent[0] + 7) / 8;
        uint32_t group_size_y = (extent[1] + 7) / 8;
        api.cb().dispatch(group_size_x, group_size_y, 1);
    });
}

// Bezold-Brücke LUT Computer Implementation

std::shared_ptr<vulkan::Image> BezoldBruckeLutComputer::create(vulkan::Device* device) {
    auto desc = vulkan::ImageDesc::new_2d(64, 1, vk::Format::eR16G16Sfloat)
        .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);

    return device->create_image(desc, "bezold_brucke_lut");
}

void BezoldBruckeLutComputer::compute(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& img
) {
    auto pass = rg.add_pass("bezold_brucke lut");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/lut/bezold_brucke.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register write to output image
    auto img_ref = pass.write(img, vulkan::AccessType::ComputeShaderWrite);

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline with output image
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {render_graph::RenderPassBinding::Image(img_ref)})
        );

        // Dispatch compute shader
        auto extent = img_ref.desc().extent;
        uint32_t group_size_x = (extent[0] + 7) / 8;
        uint32_t group_size_y = (extent[1] + 7) / 8;
        api.cb().dispatch(group_size_x, group_size_y, 1);
    });
}

} // namespace tekki::renderer
