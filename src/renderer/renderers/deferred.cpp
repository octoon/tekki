#include "tekki/renderer/renderers/deferred.h"
#include "tekki/renderer/render_graph/render_graph.h"
#include "tekki/renderer/render_graph/simple_render_pass.h"
#include "tekki/renderer/renderers/ircache_render_state.h"
#include "tekki/renderer/renderers/wrc_render_state.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace tekki::renderer::renderers {

/**
 * Light GBuffer rendering pass
 */
void LightGbuffer(
    RenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<Image>& shadowMask,
    const std::shared_ptr<Image>& rtr,
    const std::shared_ptr<Image>& rtdgi,
    IrcacheRenderState& ircache,
    const WrcRenderState& wrc,
    std::shared_ptr<Image>& temporalOutput,
    std::shared_ptr<Image>& output,
    const std::shared_ptr<Image>& skyCube,
    const std::shared_ptr<Image>& convolvedSkyCube,
    vk::DescriptorSet bindlessDescriptorSet,
    size_t debugShadingMode,
    bool debugShowWrc
) {
    try {
        auto pass = rg.add_pass("light gbuffer");
        SimpleRenderPass renderPass(pass, "/shaders/light_gbuffer.hlsl", SimpleRenderPass::Type::COMPUTE);
        
        renderPass.read(gbufferDepth.gbuffer);
        renderPass.read_aspect(gbufferDepth.depth, vk::ImageAspectFlagBits::eDepth);
        renderPass.read(shadowMask);
        renderPass.read(rtr);
        renderPass.read(rtdgi);
        renderPass.bind_mut(ircache);
        renderPass.bind(wrc);
        renderPass.write(temporalOutput);
        renderPass.write(output);
        renderPass.read(skyCube);
        renderPass.read(convolvedSkyCube);
        
        auto extent = gbufferDepth.gbuffer->get_desc().extent;
        glm::vec4 invExtent(1.0f / extent.width, 1.0f / extent.height, extent.width, extent.height);
        
        struct Constants {
            glm::vec4 extentInvExtent;
            uint32_t debugShadingMode;
            uint32_t debugShowWrc;
        } constants;
        
        constants.extentInvExtent = invExtent;
        constants.debugShadingMode = static_cast<uint32_t>(debugShadingMode);
        constants.debugShowWrc = static_cast<uint32_t>(debugShowWrc);
        
        renderPass.constants(constants);
        renderPass.raw_descriptor_set(1, bindlessDescriptorSet);
        renderPass.dispatch(extent);
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("LightGbuffer failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers