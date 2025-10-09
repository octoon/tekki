#include "tekki/renderer/renderers/sky.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace tekki::renderer::renderers {

/**
 * Render sky cube
 * @param rg The render graph
 * @return Handle to the created sky texture
 */
tekki::render_graph::Handle<tekki::backend::vulkan::Image> SkyRenderer::RenderSkyCube(tekki::render_graph::RenderGraph& rg) {
    try {
        uint32_t width = 64;
        
        tekki::backend::vulkan::ImageDesc desc;
        desc.ImageType = tekki::backend::vulkan::ImageType::Cube;
        desc.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
        desc.Width = width;
        desc.Height = width;
        desc.Depth = 1;
        desc.ArrayLayers = 6;
        desc.MipLevels = 1;
        desc.SampleCount = VK_SAMPLE_COUNT_1_BIT;
        desc.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.Flags = 0;
        
        auto sky_tex = rg.create(desc);

        auto pass = rg.add_pass("sky cube");
        
        tekki::backend::vulkan::ImageViewDesc view_desc;
        view_desc.ViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        
        auto render_pass = tekki::render_graph::SimpleRenderPass::new_compute(pass, "/shaders/sky/comp_cube.hlsl");
        render_pass.write_view(sky_tex, view_desc);
        render_pass.dispatch({width, width, 6});

        return sky_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to render sky cube: ") + e.what());
    }
}

/**
 * Convolve cube map
 * @param rg The render graph
 * @param input Input cube map texture
 * @return Handle to the convolved sky texture
 */
tekki::render_graph::Handle<tekki::backend::vulkan::Image> SkyRenderer::ConvolveCube(
    tekki::render_graph::RenderGraph& rg,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input
) {
    try {
        uint32_t width = 16;
        
        tekki::backend::vulkan::ImageDesc desc;
        desc.ImageType = tekki::backend::vulkan::ImageType::Cube;
        desc.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
        desc.Width = width;
        desc.Height = width;
        desc.Depth = 1;
        desc.ArrayLayers = 6;
        desc.MipLevels = 1;
        desc.SampleCount = VK_SAMPLE_COUNT_1_BIT;
        desc.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.Flags = 0;
        
        auto sky_tex = rg.create(desc);

        auto pass = rg.add_pass("convolve sky");
        
        tekki::backend::vulkan::ImageViewDesc view_desc;
        view_desc.ViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        
        auto render_pass = tekki::render_graph::SimpleRenderPass::new_compute(pass, "/shaders/convolve_cube.hlsl");
        render_pass.read(input);
        render_pass.write_view(sky_tex, view_desc);
        render_pass.constants(width);
        render_pass.dispatch({width, width, 6});

        return sky_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to convolve cube map: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers