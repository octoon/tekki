#include "tekki/renderer/renderers/sky.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/backend/vk_sync.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

/**
 * Render sky cube
 * @param renderGraph The render graph
 * @return Handle to the created sky texture
 */
rg::Handle<rg::Image> SkyRenderer::RenderSkyCube(rg::RenderGraph& renderGraph) {
    try {
        uint32_t width = 64;

        // Create cube texture using NewCube
        auto desc = rg::ImageDesc::NewCube(VK_FORMAT_R16G16B16A16_SFLOAT, width)
            .WithUsage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        auto sky_tex = renderGraph.Create(desc);

        rg::PassBuilder pass = renderGraph.AddPass("sky cube");
        auto pipeline = pass.RegisterComputePipeline("/shaders/sky/comp_cube.hlsl");
        auto sky_tex_ref = pass.Write(sky_tex, vk_sync::AccessType::ComputeShaderWrite);

        pass.Render([pipeline, sky_tex_ref, width](rg::RenderPassApi& api) {
            // TODO: Implement actual rendering when RenderPassApi is complete
            // This is a placeholder for compute shader dispatch
            (void)pipeline;
            (void)sky_tex_ref;
            (void)width;
            (void)api;
        });

        return sky_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to render sky cube: ") + e.what());
    }
}

/**
 * Convolve cube map
 * @param renderGraph The render graph
 * @param input Input cube map texture
 * @return Handle to the convolved sky texture
 */
rg::Handle<rg::Image> SkyRenderer::ConvolveCube(
    rg::RenderGraph& renderGraph,
    const rg::Handle<rg::Image>& input
) {
    try {
        uint32_t width = 16;

        auto desc = rg::ImageDesc::NewCube(VK_FORMAT_R16G16B16A16_SFLOAT, width)
            .WithUsage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        auto sky_tex = renderGraph.Create(desc);

        rg::PassBuilder pass = renderGraph.AddPass("convolve sky");
        auto pipeline = pass.RegisterComputePipeline("/shaders/convolve_cube.hlsl");
        auto input_ref = pass.Read(input, vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer);
        auto sky_tex_ref = pass.Write(sky_tex, vk_sync::AccessType::ComputeShaderWrite);

        pass.Render([pipeline, input_ref, sky_tex_ref, width](rg::RenderPassApi& api) {
            // TODO: Implement actual rendering when RenderPassApi is complete
            // This is a placeholder for compute shader dispatch
            (void)pipeline;
            (void)input_ref;
            (void)sky_tex_ref;
            (void)width;
            (void)api;
        });

        return sky_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to convolve cube map: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers
