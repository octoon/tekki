#include "tekki/renderer/renderers/raster_meshes.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <stdexcept>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/renderer/render_graph.h"
#include "tekki/renderer/render_pass.h"
#include "tekki/renderer/gbuffer_depth.h"

namespace tekki::renderer::renderers {

void RasterMeshes::RasterMeshes(
    class RenderGraph& renderGraph,
    std::shared_ptr<class RenderPass> renderPass,
    class GbufferDepth& gbufferDepth,
    class Handle<class Image>& velocityImage,
    const RasterMeshesData& meshData
) {
    try {
        auto& pass = renderGraph.add_pass("raster simple");

        std::vector<PipelineShaderDesc> shaderDescs = {
            PipelineShaderDesc::builder(ShaderPipelineStage::Vertex)
                .hlsl_source("/shaders/raster_simple_vs.hlsl")
                .build(),
            PipelineShaderDesc::builder(ShaderPipelineStage::Pixel)
                .hlsl_source("/shaders/raster_simple_ps.hlsl")
                .build()
        };

        auto pipeline = pass.register_raster_pipeline(
            shaderDescs,
            RasterPipelineDesc::builder()
                .render_pass(renderPass)
                .face_cull(false)
                .push_constants_bytes(2 * sizeof(uint32_t))
        );

        std::vector<UploadedTriMesh> meshes = *meshData.Meshes;
        std::vector<class MeshInstance> instances = *meshData.Instances;

        auto depth_ref = pass.raster(
            &gbufferDepth.depth,
            AccessType::DepthAttachmentWriteStencilReadOnly
        );

        auto geometric_normal_ref = pass.raster(
            &gbufferDepth.geometric_normal,
            AccessType::ColorAttachmentWrite
        );
        auto gbuffer_ref = pass.raster(&gbufferDepth.gbuffer, AccessType::ColorAttachmentWrite);
        auto velocity_ref = pass.raster(&velocityImage, AccessType::ColorAttachmentWrite);

        auto vertex_buffer = meshData.VertexBuffer;
        auto bindless_descriptor_set = meshData.BindlessDescriptorSet;

        pass.render([=, &instances, &meshes](RenderAPI& api) {
            auto extent = gbuffer_ref.desc().extent;
            uint32_t width = extent[0];
            uint32_t height = extent[1];

            std::vector<std::array<float, 12>> transforms;
            std::vector<std::array<float, 12>> prev_transforms;
            
            for (const auto& inst : instances) {
                std::array<float, 12> transform = {
                    inst.transform.x_axis.x,
                    inst.transform.y_axis.x,
                    inst.transform.z_axis.x,
                    inst.transform.translation.x,
                    inst.transform.x_axis.y,
                    inst.transform.y_axis.y,
                    inst.transform.z_axis.y,
                    inst.transform.translation.y,
                    inst.transform.x_axis.z,
                    inst.transform.y_axis.z,
                    inst.transform.z_axis.z,
                    inst.transform.translation.z
                };

                std::array<float, 12> prev_transform = {
                    inst.prev_transform.x_axis.x,
                    inst.prev_transform.y_axis.x,
                    inst.prev_transform.z_axis.x,
                    inst.prev_transform.translation.x,
                    inst.prev_transform.x_axis.y,
                    inst.prev_transform.y_axis.y,
                    inst.prev_transform.z_axis.y,
                    inst.prev_transform.translation.y,
                    inst.prev_transform.x_axis.z,
                    inst.prev_transform.y_axis.z,
                    inst.prev_transform.z_axis.z,
                    inst.prev_transform.translation.z
                };

                transforms.push_back(transform);
                prev_transforms.push_back(prev_transform);
            }

            auto instance_transforms_offset = api.dynamic_constants().push_from_iter(
                transforms.begin(), transforms.end()
            );

            std::vector<std::pair<ImageHandle, ImageViewDesc>> colorAttachments = {
                {geometric_normal_ref, ImageViewDesc{}},
                {gbuffer_ref, ImageViewDesc{}},
                {velocity_ref, ImageViewDesc{}}
            };

            api.begin_render_pass(
                renderPass,
                {width, height},
                colorAttachments,
                std::make_optional(std::make_pair(
                    depth_ref,
                    ImageViewDesc::builder()
                        .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                        .build()
                ))
            );

            api.set_default_view_and_scissor({width, height});

            auto bound_pipeline = api.bind_raster_pipeline(
                pipeline
                    .into_binding()
                    .descriptor_set(
                        0,
                        {RenderPassBinding::DynamicConstantsStorageBuffer(
                            instance_transforms_offset
                        )}
                    )
                    .raw_descriptor_set(1, bindless_descriptor_set)
            );

            auto raw_device = &api.device().raw;
            auto command_buffer = api.command_buffer;

            for (size_t draw_idx = 0; draw_idx < instances.size(); ++draw_idx) {
                const auto& instance = instances[draw_idx];
                const auto& mesh = meshes[instance.mesh.index];

                vkCmdBindIndexBuffer(
                    command_buffer.raw,
                    vertex_buffer->Raw,
                    mesh.IndexBufferOffset,
                    VK_INDEX_TYPE_UINT32
                );

                std::array<uint32_t, 2> push_constants = {
                    static_cast<uint32_t>(draw_idx),
                    static_cast<uint32_t>(instance.mesh.index)
                };

                bound_pipeline.push_constants(
                    command_buffer.raw,
                    VK_SHADER_STAGE_ALL_GRAPHICS,
                    0,
                    sizeof(push_constants),
                    push_constants.data()
                );

                vkCmdDrawIndexed(
                    command_buffer.raw,
                    mesh.IndexCount,
                    1,
                    0,
                    0,
                    0
                );
            }

            api.end_render_pass();
        });
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("RasterMeshes failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers