#include "tekki/renderer/renderers/dof.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <stdexcept>
#include "tekki/backend/vulkan/image.h"
#include "render_graph.h"

namespace tekki::renderer::renderers {

std::shared_ptr<tekki::backend::vulkan::Image> Dof::Apply(
    std::shared_ptr<RenderGraph> renderGraph,
    std::shared_ptr<tekki::backend::vulkan::Image> input,
    std::shared_ptr<tekki::backend::vulkan::Image> depth
) {
    try {
        // Create CoC image
        auto cocDesc = tekki::backend::vulkan::ImageDesc{
            .ImageType = tekki::backend::vulkan::ImageType::Tex2d,
            .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Flags = 0,
            .Format = VK_FORMAT_R16_SFLOAT,
            .Extent = input->Desc.Extent,
            .MipLevels = 1,
            .ArrayLayers = 1,
            .Samples = VK_SAMPLE_COUNT_1_BIT
        };
        auto coc = renderGraph->CreateImage(cocDesc);

        // Create CoC tiles image
        auto cocTilesExtent = input->Desc.Extent;
        cocTilesExtent.width = (cocTilesExtent.width + 7) / 8;
        cocTilesExtent.height = (cocTilesExtent.height + 7) / 8;
        
        auto cocTilesDesc = tekki::backend::vulkan::ImageDesc{
            .ImageType = tekki::backend::vulkan::ImageType::Tex2d,
            .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Flags = 0,
            .Format = VK_FORMAT_R16_SFLOAT,
            .Extent = cocTilesExtent,
            .MipLevels = 1,
            .ArrayLayers = 1,
            .Samples = VK_SAMPLE_COUNT_1_BIT
        };
        auto cocTiles = renderGraph->CreateImage(cocTilesDesc);

        // CoC pass
        auto cocPass = renderGraph->AddPass("coc");
        auto cocRenderPass = std::make_shared<SimpleRenderPass>(cocPass, "/shaders/dof/coc.hlsl", RenderPassType::Compute);
        
        cocRenderPass->ReadAspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT);
        cocRenderPass->Write(coc);
        cocRenderPass->Write(cocTiles);
        cocRenderPass->Dispatch(input->Desc.Extent.width, input->Desc.Extent.height, 1);

        // Create DoF output image
        auto dofDesc = tekki::backend::vulkan::ImageDesc{
            .ImageType = tekki::backend::vulkan::ImageType::Tex2d,
            .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Flags = 0,
            .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .Extent = input->Desc.Extent,
            .MipLevels = 1,
            .ArrayLayers = 1,
            .Samples = VK_SAMPLE_COUNT_1_BIT
        };
        auto dof = renderGraph->CreateImage(dofDesc);

        // DoF gather pass
        auto dofPass = renderGraph->AddPass("dof gather");
        auto dofRenderPass = std::make_shared<SimpleRenderPass>(dofPass, "/shaders/dof/gather.hlsl", RenderPassType::Compute);
        
        dofRenderPass->ReadAspect(depth, VK_IMAGE_ASPECT_DEPTH_BIT);
        dofRenderPass->Read(input);
        dofRenderPass->Read(coc);
        dofRenderPass->Read(cocTiles);
        dofRenderPass->Write(dof);
        
        // Set constants for inverse extent
        glm::vec2 invExtent(1.0f / input->Desc.Extent.width, 1.0f / input->Desc.Extent.height);
        dofRenderPass->SetConstants(invExtent);
        
        dofRenderPass->Dispatch(input->Desc.Extent.width, input->Desc.Extent.height, 1);

        return dof;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Dof::Apply failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers