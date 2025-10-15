#include "tekki/renderer/renderers/ussgi.h"
#include <stdexcept>
#include <glm/glm.hpp>
#include "tekki/render_graph/RenderGraph.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

UssgiRenderer::UssgiRenderer()
    : ussgiTex_("ussgi") {
}

rg::Handle<Image> UssgiRenderer::Render(
    rg::TemporalRenderGraph& renderGraph,
    const std::shared_ptr<GbufferDepth>& gbufferDepth,
    const rg::Handle<Image>& reprojectionMap,
    const rg::Handle<Image>& prevRadiance,
    VkDescriptorSet bindlessDescriptorSet
) {
    try {
        auto gbufferDesc = gbufferDepth->gbuffer.desc;
        auto halfViewNormalTex = gbufferDepth->half_view_normal(*renderGraph.GetRenderGraph());

        // Create USSGI texture
        auto ussgiTex = renderGraph.Create(rg::ImageDesc::New2d(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            glm::u32vec2(gbufferDesc.Extent.x, gbufferDesc.Extent.y)
        ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT));

        // USSGI pass
        {
            auto pass = renderGraph.AddPass("ussgi");
            auto renderPass = rg::SimpleRenderPass::NewCompute(
                pass,
                "/shaders/ssgi/ussgi.hlsl"
            );
            renderPass
                .Read(gbufferDepth->gbuffer)
                .ReadAspect(gbufferDepth->depth, VK_IMAGE_ASPECT_DEPTH_BIT)
                .Read(halfViewNormalTex)
                .Read(prevRadiance)
                .Read(reprojectionMap)
                .Write(ussgiTex)
                .RawDescriptorSet(1, bindlessDescriptorSet)
                .Constants(std::make_tuple(
                    glm::vec4(gbufferDesc.Extent.x, gbufferDesc.Extent.y,
                             1.0f/gbufferDesc.Extent.x, 1.0f/gbufferDesc.Extent.y),
                    glm::vec4(ussgiTex.desc.Extent.x, ussgiTex.desc.Extent.y,
                             1.0f/ussgiTex.desc.Extent.x, 1.0f/ussgiTex.desc.Extent.y)
                ))
                .Dispatch(glm::u32vec3(ussgiTex.desc.Extent.x, ussgiTex.desc.Extent.y, 1));
        }

        // Temporal filtering
        auto temporalDesc = TemporalTexDesc(glm::uvec2(gbufferDesc.Extent.x, gbufferDesc.Extent.y));
        auto [filteredOutputTex, historyTex] = ussgiTex_.GetOutputAndHistory(renderGraph, temporalDesc);

        {
            auto pass = renderGraph.AddPass("ussgi temporal");
            auto renderPass = rg::SimpleRenderPass::NewCompute(
                pass,
                "/shaders/ssgi/temporal_filter.hlsl"
            );
            renderPass
                .Read(ussgiTex)
                .Read(historyTex)
                .Read(reprojectionMap)
                .Write(filteredOutputTex)
                .Constants(std::make_tuple(
                    glm::vec4(filteredOutputTex.desc.Extent.x, filteredOutputTex.desc.Extent.y,
                             1.0f/filteredOutputTex.desc.Extent.x, 1.0f/filteredOutputTex.desc.Extent.y)
                ))
                .Dispatch(glm::u32vec3(filteredOutputTex.desc.Extent.x, filteredOutputTex.desc.Extent.y, 1));
        }

        return rg::Handle<tekki::backend::vulkan::Image>(filteredOutputTex);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("UssgiRenderer::Render failed: ") + e.what());
    }
}

rg::ImageDesc UssgiRenderer::TemporalTexDesc(const glm::uvec2& extent) {
    return rg::ImageDesc::New2d(VK_FORMAT_R16G16B16A16_SFLOAT, extent)
        .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

} // namespace tekki::renderer::renderers