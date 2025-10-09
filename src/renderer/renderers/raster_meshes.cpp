#include "../../../include/tekki/renderer/renderers/raster_meshes.h"
#include "../../../include/tekki/backend/vulkan/shader.h"
#include "../../../include/tekki/backend/vulkan/dynamic_constants.h"

namespace tekki::renderer {

void raster_meshes(
    render_graph::RenderGraph& rg,
    std::shared_ptr<vulkan::RenderPass> render_pass,
    GbufferDepth& gbuffer_depth,
    render_graph::Handle<vulkan::Image>& velocity_img,
    const RasterMeshesData& mesh_data
) {
    // Create pass builder
    auto pass = rg.add_pass("raster simple");

    // Register raster pipeline
    std::vector<vulkan::PipelineShaderDesc> shader_descs = {
        vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Vertex)
            .hlsl_source("/shaders/raster_simple_vs.hlsl")
            .build(),
        vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Pixel)
            .hlsl_source("/shaders/raster_simple_ps.hlsl")
            .build()
    };

    auto pipeline_desc = vulkan::RasterPipelineDesc::builder()
        .render_pass(render_pass)
        .face_cull(false)
        .push_constants_bytes(2 * sizeof(uint32_t));

    auto pipeline = pass.register_raster_pipeline(shader_descs, pipeline_desc);

    // Copy mesh and instance data for the lambda
    std::vector<std::shared_ptr<UploadedTriMesh>> meshes = mesh_data.meshes;
    std::vector<MeshInstance> instances = mesh_data.instances;
    auto vertex_buffer = mesh_data.vertex_buffer;
    vk::DescriptorSet bindless_descriptor_set = mesh_data.bindless_descriptor_set;

    // Register resources
    auto depth_ref = pass.raster(
        gbuffer_depth.depth,
        vulkan::AccessType::DepthAttachmentWriteStencilReadOnly
    );

    auto geometric_normal_ref = pass.raster(
        gbuffer_depth.geometric_normal,
        vulkan::AccessType::ColorAttachmentWrite
    );

    auto gbuffer_ref = pass.raster(
        gbuffer_depth.gbuffer,
        vulkan::AccessType::ColorAttachmentWrite
    );

    auto velocity_ref = pass.raster(
        velocity_img,
        vulkan::AccessType::ColorAttachmentWrite
    );

    // Render callback
    pass.render([=](render_graph::PassApi& api) {
        auto extent = gbuffer_ref.desc().extent;
        uint32_t width = extent[0];
        uint32_t height = extent[1];

        // Upload instance transforms to dynamic constants
        std::vector<std::pair<std::array<float, 12>, std::array<float, 12>>> transforms;
        transforms.reserve(instances.size());

        for (const auto& inst : instances) {
            // Current transform
            std::array<float, 12> transform = {
                inst.transform[0][0], inst.transform[1][0], inst.transform[2][0], inst.transform[3][0],
                inst.transform[0][1], inst.transform[1][1], inst.transform[2][1], inst.transform[3][1],
                inst.transform[0][2], inst.transform[1][2], inst.transform[2][2], inst.transform[3][2]
            };

            // Previous transform
            std::array<float, 12> prev_transform = {
                inst.prev_transform[0][0], inst.prev_transform[1][0], inst.prev_transform[2][0], inst.prev_transform[3][0],
                inst.prev_transform[0][1], inst.prev_transform[1][1], inst.prev_transform[2][1], inst.prev_transform[3][1],
                inst.prev_transform[0][2], inst.prev_transform[1][2], inst.prev_transform[2][2], inst.prev_transform[3][2]
            };

            transforms.push_back({transform, prev_transform});
        }

        uint32_t instance_transforms_offset = api.dynamic_constants().push_from_iter(transforms.begin(), transforms.end());

        // Begin render pass
        std::vector<std::pair<render_graph::Ref<vulkan::Image>, vulkan::ImageViewDesc>> color_attachments = {
            {geometric_normal_ref, vulkan::ImageViewDesc::default_()},
            {gbuffer_ref, vulkan::ImageViewDesc::default_()},
            {velocity_ref, vulkan::ImageViewDesc::default_()}
        };

        auto depth_view_desc = vulkan::ImageViewDesc::builder()
            .aspect_mask(vk::ImageAspectFlagBits::eDepth)
            .build();

        api.begin_render_pass(
            render_pass,
            {width, height},
            color_attachments,
            std::make_optional(std::make_pair(depth_ref, depth_view_desc))
        );

        api.set_default_view_and_scissor({width, height});

        // Bind pipeline
        auto bound_pipeline = api.bind_raster_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {render_graph::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset)})
                .raw_descriptor_set(1, bindless_descriptor_set)
        );

        // Draw all instances
        auto cb = api.cb();
        auto device = api.device();

        for (size_t draw_idx = 0; draw_idx < instances.size(); ++draw_idx) {
            const auto& instance = instances[draw_idx];
            const auto& mesh = meshes[instance.mesh.id];

            // Bind index buffer
            cb.bindIndexBuffer(
                vertex_buffer->raw(),
                mesh->index_buffer_offset,
                vk::IndexType::eUint32
            );

            // Push constants: draw_idx and mesh_idx
            struct PushConstants {
                uint32_t draw_idx;
                uint32_t mesh_idx;
            } push_constants = {
                static_cast<uint32_t>(draw_idx),
                static_cast<uint32_t>(instance.mesh.id)
            };

            bound_pipeline.push_constants(
                cb,
                vk::ShaderStageFlagBits::eAllGraphics,
                0,
                sizeof(PushConstants),
                &push_constants
            );

            // Draw indexed
            cb.drawIndexed(mesh->index_count, 1, 0, 0, 0);
        }

        api.end_render_pass();
    });
}

} // namespace tekki::renderer
