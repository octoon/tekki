#include "tekki/renderer/renderers/ussgi.h"
#include <stdexcept>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/image.h"
#include "tekki/renderer/render_graph/render_graph.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

UssgiRenderer::UssgiRenderer() 
    : ussgiTex("ussgi") {
}

std::shared_ptr<tekki::backend::vulkan::Image> UssgiRenderer::Render(
    tekki::renderer::RenderGraph& renderGraph,
    const std::shared_ptr<GbufferDepth>& gbufferDepth,
    const std::shared_ptr<tekki::backend::vulkan::Image>& reprojectionMap,
    const std::shared_ptr<tekki::backend::vulkan::Image>& prevRadiance,
    VkDescriptorSet bindlessDescriptorSet
) {
    try {
        const auto& gbufferDesc = gbufferDepth->GetGbuffer()->GetDesc();
        auto halfViewNormalTex = gbufferDepth->GetHalfViewNormal(renderGraph);

        auto ussgiTexDesc = gbufferDesc;
        ussgiTexDesc.usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
        ussgiTexDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        
        auto ussgiTex = renderGraph.CreateImage(ussgiTexDesc);

        auto ussgiPass = renderGraph.AddPass("ussgi");
        auto ussgiShader = std::make_shared<tekki::backend::vulkan::Shader>("/shaders/ssgi/ussgi.hlsl");
        
        ussgiPass->AddReadResource(gbufferDepth->GetGbuffer());
        ussgiPass->AddReadResource(gbufferDepth->GetDepth(), VK_IMAGE_ASPECT_DEPTH_BIT);
        ussgiPass->AddReadResource(halfViewNormalTex);
        ussgiPass->AddReadResource(prevRadiance);
        ussgiPass->AddReadResource(reprojectionMap);
        ussgiPass->AddWriteResource(ussgiTex);
        ussgiPass->SetDescriptorSet(1, bindlessDescriptorSet);
        
        glm::vec4 gbufferExtentInvExtent = gbufferDesc.GetExtentInvExtent2D();
        glm::vec4 ussgiExtentInvExtent = ussgiTex->GetDesc().GetExtentInvExtent2D();
        ussgiPass->SetConstants(std::vector<float>{
            gbufferExtentInvExtent.x, gbufferExtentInvExtent.y, gbufferExtentInvExtent.z, gbufferExtentInvExtent.w,
            ussgiExtentInvExtent.x, ussgiExtentInvExtent.y, ussgiExtentInvExtent.z, ussgiExtentInvExtent.w
        });
        
        ussgiPass->Dispatch(ussgiTex->GetDesc().extent.width, ussgiTex->GetDesc().extent.height, 1);

        auto temporalDesc = TemporalTexDesc(glm::uvec2(gbufferDesc.extent.width, gbufferDesc.extent.height));
        auto [filteredOutputTex, historyTex] = ussgiTex.GetOutputAndHistory(renderGraph, temporalDesc);

        auto temporalPass = renderGraph.AddPass("ussgi temporal");
        auto temporalShader = std::make_shared<tekki::backend::vulkan::Shader>("/shaders/ssgi/temporal_filter.hlsl");
        
        temporalPass->AddReadResource(ussgiTex);
        temporalPass->AddReadResource(historyTex);
        temporalPass->AddReadResource(reprojectionMap);
        temporalPass->AddWriteResource(filteredOutputTex);
        
        glm::vec4 filteredExtentInvExtent = filteredOutputTex->GetDesc().GetExtentInvExtent2D();
        temporalPass->SetConstants(std::vector<float>{
            filteredExtentInvExtent.x, filteredExtentInvExtent.y, filteredExtentInvExtent.z, filteredExtentInvExtent.w
        });
        
        temporalPass->Dispatch(filteredOutputTex->GetDesc().extent.width, filteredOutputTex->GetDesc().extent.height, 1);

        return filteredOutputTex;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("UssgiRenderer::Render failed: ") + e.what());
    }
}

tekki::backend::vulkan::ImageDesc UssgiRenderer::TemporalTexDesc(const glm::uvec2& extent) {
    tekki::backend::vulkan::ImageDesc desc;
    desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.extent = {extent.x, extent.y, 1};
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    return desc;
}

} // namespace tekki::renderer::renderers