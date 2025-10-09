#include "tekki/renderer/renderers/half_res.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace tekki::renderer::renderers {

/**
 * Extract half resolution GBuffer view normal as RGBA8
 * @param rg The render graph
 * @param gbuffer The GBuffer image handle
 * @return Half resolution view normal image
 */
std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> HalfRes::ExtractHalfResGbufferViewNormalRgba8(
    tekki::render_graph::RenderGraph& rg,
    const std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>>& gbuffer
) {
    try {
        auto gbuffer_desc = gbuffer->desc();
        auto half_res_desc = gbuffer_desc.half_res()
            .usage(VkImageUsageFlags(0))
            .format(VK_FORMAT_R8G8B8A8_SNORM);
        
        auto output_tex = rg.create(half_res_desc);
        
        auto pass = rg.add_pass("extract view normal/2");
        auto render_pass = tekki::render_graph::SimpleRenderPass::new_compute(
            pass,
            "/shaders/extract_half_res_gbuffer_view_normal_rgba8.hlsl"
        );
        
        render_pass.read(gbuffer)
                  .write(output_tex)
                  .dispatch(output_tex->desc().extent);
        
        return output_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to extract half resolution GBuffer view normal: ") + e.what());
    }
}

/**
 * Extract half resolution depth
 * @param rg The render graph
 * @param depth The depth image handle
 * @return Half resolution depth image
 */
std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> HalfRes::ExtractHalfResDepth(
    tekki::render_graph::RenderGraph& rg,
    const std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>>& depth
) {
    try {
        auto depth_desc = depth->desc();
        auto half_res_desc = depth_desc.half_res()
            .usage(VkImageUsageFlags(0))
            .format(VK_FORMAT_R32_SFLOAT);
        
        auto output_tex = rg.create(half_res_desc);
        
        auto pass = rg.add_pass("extract half depth");
        auto render_pass = tekki::render_graph::SimpleRenderPass::new_compute(
            pass,
            "/shaders/extract_half_res_depth.hlsl"
        );
        
        render_pass.read_aspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT)
                  .write(output_tex)
                  .dispatch(output_tex->desc().extent);
        
        return output_tex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to extract half resolution depth: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers