#include "tekki/renderer/renderers/old/sdf.h"
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "RenderGraph.h"
#include "Image.h"
#include "Buffer.h"
#include "RenderPass.h"

namespace tekki::renderer::renderers::old {

std::shared_ptr<Image> SdfRenderer::RaymarchSdf(
    std::shared_ptr<RenderGraph> renderGraph,
    std::shared_ptr<Image> sdfImage,
    ImageDesc description
) {
    auto pass = renderGraph->add_pass();

    auto pipeline = pass->register_compute_pipeline("/shaders/sdf/sdf_raymarch_gbuffer.hlsl");

    auto sdf_ref = pass->read(
        sdfImage,
        AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
    );
    auto output = pass->create(description);
    auto output_ref = pass->write(output, AccessType::ComputeShaderWrite);

    pass->render([=](auto api) {
        auto bound_pipeline = api->bind_compute_pipeline(pipeline->into_binding()->descriptor_set(
            0,
            {
                output_ref->bind(std::make_shared<ImageViewDescBuilder>()->build()),
                sdf_ref->bind(std::make_shared<ImageViewDescBuilder>()->build())
            }
        ));

        bound_pipeline->dispatch(description.extent);
    });

    return output;
}

void SdfRenderer::EditSdf(
    std::shared_ptr<RenderGraph> renderGraph,
    std::shared_ptr<Image> sdfImage,
    bool clear
) {
    auto pass = renderGraph->add_pass();

    auto sdf_img_ref = pass->write(sdfImage, AccessType::ComputeShaderWrite);

    std::string pipeline_path;
    if (clear) {
        pipeline_path = "/shaders/sdf/gen_empty_sdf.hlsl";
    } else {
        pipeline_path = "/shaders/sdf/edit_sdf.hlsl";
    }

    auto pipeline = pass->register_compute_pipeline(pipeline_path);

    pass->render([=](auto api) {
        auto bound_pipeline = api->bind_compute_pipeline(
            pipeline->into_binding()->descriptor_set(0, {sdf_img_ref->bind(std::make_shared<ImageViewDescBuilder>()->build())})
        );
        bound_pipeline->dispatch({SDF_DIM, SDF_DIM, SDF_DIM});
    });
}

std::shared_ptr<Buffer> SdfRenderer::ClearSdfBricksMeta(
    std::shared_ptr<RenderGraph> renderGraph
) {
    auto pass = renderGraph->add_pass();

    BufferDesc brick_meta_desc;
    brick_meta_desc.size = 20; // size of VkDrawIndexedIndirectCommand
    brick_meta_desc.usage = vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::INDIRECT_BUFFER;

    auto brick_meta_buf = pass->create(brick_meta_desc);
    auto brick_meta_buf_ref = pass->write(brick_meta_buf, AccessType::ComputeShaderWrite);

    auto clear_meta_pipeline = pass->register_compute_pipeline("/shaders/sdf/clear_bricks_meta.hlsl");

    pass->render([=](auto api) {
        auto bound_pipeline = api->bind_compute_pipeline(
            clear_meta_pipeline->into_binding()->descriptor_set(0, {brick_meta_buf_ref->bind()})
        );
        bound_pipeline->dispatch({1, 1, 1});
    });

    return brick_meta_buf;
}

SdfRasterBricks SdfRenderer::CalculateSdfBricksMeta(
    std::shared_ptr<RenderGraph> renderGraph,
    std::shared_ptr<Image> sdfImage
) {
    auto brick_meta_buf = ClearSdfBricksMeta(renderGraph);

    auto pass = renderGraph->add_pass();

    auto sdf_ref = pass->read(
        sdfImage,
        AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
    );

    auto brick_meta_buf_ref = pass->write(brick_meta_buf, AccessType::ComputeShaderWrite);

    BufferDesc brick_inst_desc;
    brick_inst_desc.size = std::pow(SDF_DIM, 3) * 4 * 4;
    brick_inst_desc.usage = vk::BufferUsageFlags::STORAGE_BUFFER;

    auto brick_inst_buf = pass->create(brick_inst_desc);
    auto brick_inst_buf_ref = pass->write(brick_inst_buf, AccessType::ComputeShaderWrite);

    auto calc_meta_pipeline = pass->register_compute_pipeline("/shaders/sdf/find_bricks.hlsl");

    pass->render([=](auto api) {
        auto bound_pipeline = api->bind_compute_pipeline(calc_meta_pipeline->into_binding()->descriptor_set(
            0,
            {
                sdf_ref->bind(std::make_shared<ImageViewDescBuilder>()->build()),
                brick_meta_buf_ref->bind(),
                brick_inst_buf_ref->bind()
            }
        ));
        bound_pipeline->dispatch({SDF_DIM / 2, SDF_DIM / 2, SDF_DIM / 2});
    });

    SdfRasterBricks result;
    result.BrickMetaBuffer = brick_meta_buf;
    result.BrickInstBuffer = brick_inst_buf;
    return result;
}

void SdfRenderer::RasterSdf(
    std::shared_ptr<RenderGraph> renderGraph,
    std::shared_ptr<RenderPass> renderPass,
    std::shared_ptr<Image> depthImage,
    std::shared_ptr<Image> colorImage,
    RasterSdfData rasterSdfData
) {
    auto pass = renderGraph->add_pass();

    std::vector<RasterPipelineShader> shaders = {
        RasterPipelineShader{
            "/shaders/raster_simple_vs.hlsl",
            RasterShaderDesc::builder(RasterStage::Vertex)->build()
        },
        RasterPipelineShader{
            "/shaders/raster_simple_ps.hlsl",
            RasterShaderDesc::builder(RasterStage::Pixel)->build()
        }
    };

    auto pipeline_desc = RasterPipelineDesc::builder()
        ->render_pass(renderPass)
        ->face_cull(true);

    auto pipeline = pass->register_raster_pipeline(shaders, pipeline_desc->build());

    auto sdf_ref = pass->read(
        rasterSdfData.SdfImg,
        AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer
    );
    auto brick_inst_buffer = pass->read(
        rasterSdfData.BrickInstBuffer,
        AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer
    );
    auto brick_meta_buffer = pass->read(
        rasterSdfData.BrickMetaBuffer,
        AccessType::IndirectBuffer
    );
    auto cube_index_buffer = pass->read(rasterSdfData.CubeIndexBuffer, AccessType::IndexBuffer);

    auto depth_ref = pass->raster(depthImage, AccessType::DepthAttachmentWriteStencilReadOnly);
    auto color_ref = pass->raster(colorImage, AccessType::ColorAttachmentWrite);

    pass->render([=](auto api) {
        auto extent = color_ref->desc().extent;
        uint32_t width = extent[0];
        uint32_t height = extent[1];

        auto depth_view_desc = ImageViewDesc::builder()
            ->aspect_mask(vk::ImageAspectFlags::DEPTH)
            ->build();

        api->begin_render_pass(
            renderPass,
            {width, height},
            {{color_ref, std::make_shared<ImageViewDesc>()}},
            std::make_pair(depth_ref, depth_view_desc)
        );

        api->set_default_view_and_scissor({width, height});

        auto bound_pipeline = api->bind_raster_pipeline(pipeline->into_binding()->descriptor_set(
            0,
            {
                brick_inst_buffer->bind(),
                sdf_ref->bind(std::make_shared<ImageViewDescBuilder>()->build())
            }
        ));

        auto raw_device = api->device()->raw;
        auto cb = api->cb;

        raw_device.cmd_bind_index_buffer(
            cb.raw,
            api->resources->buffer(cube_index_buffer)->raw,
            0,
            vk::IndexType::UINT32
        );

        raw_device.cmd_draw_indexed_indirect(
            cb.raw,
            api->resources->buffer(brick_meta_buffer)->raw,
            0,
            1,
            0
        );

        api->end_render_pass();
    });
}

std::vector<uint32_t> SdfRenderer::CubeIndices() {
    std::vector<uint32_t> res;
    res.reserve(6 * 2 * 3);

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> dimensions = {
        {1, 2, 4}, {2, 4, 1}, {4, 1, 2}
    };

    for (const auto& [ndim, dim0, dim1] : dimensions) {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> bits = {
            {0, dim1, dim0}, {ndim, dim0, dim1}
        };

        for (const auto& [nbit, d0, d1] : bits) {
            res.push_back(nbit);
            res.push_back(nbit + d0);
            res.push_back(nbit + d1);

            res.push_back(nbit + d1);
            res.push_back(nbit + d0);
            res.push_back(nbit + d0 + d1);
        }
    }

    return res;
}

} // namespace tekki::renderer::renderers::old