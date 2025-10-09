#include "tekki/renderer/renderers/shadow_denoise.h"
#include <stdexcept>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/renderer/render_graph/simple_render_pass.h"
#include "tekki/renderer/resources/image.h"

namespace tekki::renderer::renderers {

ShadowDenoiseRenderer::ShadowDenoiseRenderer() 
    : accum_("shadow_denoise_accum")
    , moments_("shadow_denoise_moments") {
}

/**
 * Render shadow denoising pass
 * @param rg Temporal render graph
 * @param gbufferDepth G-buffer and depth information
 * @param shadowMask Shadow mask input
 * @param reprojectionMap Reprojection map for temporal accumulation
 * @return Denoised shadow output
 */
std::shared_ptr<tekki::renderer::Image> ShadowDenoiseRenderer::Render(
    std::shared_ptr<tekki::renderer::TemporalRenderGraph> rg,
    std::shared_ptr<tekki::renderer::GbufferDepth> gbufferDepth,
    std::shared_ptr<tekki::renderer::Image> shadowMask,
    std::shared_ptr<tekki::renderer::Image> reprojectionMap
) {
    try {
        auto gbufferDesc = gbufferDepth->GetGbuffer()->GetDesc();

        glm::uvec2 bitpackedShadowMaskExtent = gbufferDesc.DivUpExtent(glm::uvec3(8, 4, 1)).GetExtent2D();
        auto bitpackedShadowsImage = rg->CreateImage(ImageDesc::Create2D(
            vk::Format::eR32Uint,
            bitpackedShadowMaskExtent
        ));

        SimpleRenderPass::CreateCompute(
            rg->AddPass("shadow bitpack"),
            "/shaders/shadow_denoise/bitpack_shadow_mask.hlsl"
        )
        .Read(shadowMask)
        .Write(bitpackedShadowsImage)
        .Constants(std::make_tuple(
            gbufferDesc.GetExtentInvExtent2D(),
            bitpackedShadowMaskExtent
        ))
        .Dispatch(glm::uvec3(
            bitpackedShadowMaskExtent.x * 2,
            bitpackedShadowMaskExtent.y,
            1
        ));

        auto [momentsImage, prevMomentsImage] = moments_.GetOutputAndHistory(
            rg,
            gbufferDesc
                .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
        );

        auto spatialImageDesc = ImageDesc::Create2D(
            vk::Format::eR16G16Sfloat,
            gbufferDesc.GetExtent2D()
        );

        auto [accumImage, prevAccumImage] = accum_.GetOutputAndHistory(
            rg,
            spatialImageDesc.SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage)
        );

        auto spatialInputImage = rg->CreateImage(spatialImageDesc);
        auto metadataImage = rg->CreateImage(ImageDesc::Create2D(
            vk::Format::eR32Uint,
            bitpackedShadowMaskExtent
        ));

        SimpleRenderPass::CreateCompute(
            rg->AddPass("shadow temporal"),
            "/shaders/shadow_denoise/megakernel.hlsl"
        )
        .Read(shadowMask)
        .Read(bitpackedShadowsImage)
        .Read(prevMomentsImage)
        .Read(prevAccumImage)
        .Read(reprojectionMap)
        .Write(momentsImage)
        .Write(spatialInputImage)
        .Write(metadataImage)
        .Constants(std::make_tuple(
            gbufferDesc.GetExtentInvExtent2D(),
            bitpackedShadowMaskExtent
        ))
        .Dispatch(gbufferDesc.GetExtent());

        auto temp = rg->CreateImage(spatialImageDesc);
        FilterSpatial(
            rg,
            1,
            spatialInputImage,
            accumImage,
            metadataImage,
            gbufferDepth,
            bitpackedShadowMaskExtent
        );

        FilterSpatial(
            rg,
            2,
            accumImage,
            temp,
            metadataImage,
            gbufferDepth,
            bitpackedShadowMaskExtent
        );

        FilterSpatial(
            rg,
            4,
            temp,
            spatialInputImage,
            metadataImage,
            gbufferDepth,
            bitpackedShadowMaskExtent
        );

        return spatialInputImage;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ShadowDenoiseRenderer::Render failed: ") + e.what());
    }
}

/**
 * Apply spatial filtering to shadow data
 * @param rg Temporal render graph
 * @param stepSize Filter step size
 * @param inputImage Input image to filter
 * @param outputImage Output filtered image
 * @param metadataImage Metadata for filtering
 * @param gbufferDepth G-buffer and depth information
 * @param bitpackedShadowMaskExtent Extent of bitpacked shadow mask
 */
void ShadowDenoiseRenderer::FilterSpatial(
    std::shared_ptr<tekki::renderer::TemporalRenderGraph> rg,
    uint32_t stepSize,
    std::shared_ptr<tekki::renderer::Image> inputImage,
    std::shared_ptr<tekki::renderer::Image> outputImage,
    std::shared_ptr<tekki::renderer::Image> metadataImage,
    std::shared_ptr<tekki::renderer::GbufferDepth> gbufferDepth,
    glm::uvec2 bitpackedShadowMaskExtent
) {
    try {
        SimpleRenderPass::CreateCompute(
            rg->AddPass("shadow spatial"),
            "/shaders/shadow_denoise/spatial_filter.hlsl"
        )
        .Read(inputImage)
        .Read(metadataImage)
        .Read(gbufferDepth->GetGeometricNormal())
        .ReadAspect(gbufferDepth->GetDepth(), vk::ImageAspectFlagBits::eDepth)
        .Write(outputImage)
        .Constants(std::make_tuple(
            outputImage->GetDesc().GetExtentInvExtent2D(),
            bitpackedShadowMaskExtent,
            stepSize
        ))
        .Dispatch(outputImage->GetDesc().GetExtent());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ShadowDenoiseRenderer::FilterSpatial failed: ") + e.what());
    }
}

} // namespace tekki::renderer::renderers