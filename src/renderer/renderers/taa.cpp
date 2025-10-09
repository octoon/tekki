#include "tekki/renderer/renderers/taa.h"
#include <glm/glm.hpp>
#include <array>
#include <stdexcept>

namespace tekki::renderer::renderers {

TaaRenderer::TaaRenderer() 
    : temporal_tex("taa")
    , temporal_velocity_tex("taa.velocity")
    , temporal_smooth_var_tex("taa.smooth_var")
    , CurrentSupersampleOffset(0.0f, 0.0f) {
}

ImageDesc TaaRenderer::TemporalTexDesc(const std::array<uint32_t, 2>& extent) {
    return ImageDesc::new_2d(VK_FORMAT_R16G16B16A16_SFLOAT, extent)
        .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

TaaOutput TaaRenderer::Render(
    rg::TemporalRenderGraph& rg,
    const rg::Handle<Image>& inputTex,
    const rg::Handle<Image>& reprojectionMap,
    const rg::Handle<Image>& depthTex,
    const std::array<uint32_t, 2>& outputExtent
) {
    auto [temporalOutputTex, historyTex] = temporal_tex.GetOutputAndHistory(rg, TemporalTexDesc(outputExtent));

    auto [temporalVelocityOutputTex, velocityHistoryTex] = temporal_velocity_tex.GetOutputAndHistory(
        rg,
        ImageDesc::new_2d(VK_FORMAT_R16G16_SFLOAT, outputExtent)
            .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    auto reprojectedHistoryImg = rg.Create(TemporalTexDesc(outputExtent));
    auto closestVelocityImg = rg.Create(ImageDesc::new_2d(VK_FORMAT_R16G16_SFLOAT, outputExtent));

    SimpleRenderPass::NewCompute(
        rg.AddPass("reproject taa"),
        "/shaders/taa/reproject_history.hlsl"
    )
    .Read(&historyTex)
    .Read(reprojectionMap)
    .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
    .Write(&reprojectedHistoryImg)
    .Write(&closestVelocityImg)
    .Constants(std::make_tuple(
        inputTex.Desc().ExtentInvExtent2D(),
        reprojectedHistoryImg.Desc().ExtentInvExtent2D()
    ))
    .Dispatch(reprojectedHistoryImg.Desc().Extent());

    auto [smoothVarOutputTex, smoothVarHistoryTex] = temporal_smooth_var_tex.GetOutputAndHistory(
        rg,
        ImageDesc::new_2d(VK_FORMAT_R16G16B16A16_SFLOAT, outputExtent)
            .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    auto filteredInputImg = rg.Create(ImageDesc::new_2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        inputTex.Desc().Extent2D()
    ));

    auto filteredInputDeviationImg = rg.Create(ImageDesc::new_2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        inputTex.Desc().Extent2D()
    ));

    SimpleRenderPass::NewCompute(
        rg.AddPass("taa filter input"),
        "/shaders/taa/filter_input.hlsl"
    )
    .Read(inputTex)
    .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
    .Write(&filteredInputImg)
    .Write(&filteredInputDeviationImg)
    .Dispatch(filteredInputImg.Desc().Extent());

    auto filteredHistoryImg = rg.Create(ImageDesc::new_2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        filteredInputImg.Desc().Extent2D()
    ));
    
    SimpleRenderPass::NewCompute(
        rg.AddPass("taa filter history"),
        "/shaders/taa/filter_history.hlsl"
    )
    .Read(&reprojectedHistoryImg)
    .Write(&filteredHistoryImg)
    .Constants(std::make_tuple(
        reprojectedHistoryImg.Desc().ExtentInvExtent2D(),
        inputTex.Desc().ExtentInvExtent2D()
    ))
    .Dispatch(filteredHistoryImg.Desc().Extent());

    auto inputProbImg = [&]() -> rg::Handle<Image> {
        auto inputProbImg = rg.Create(ImageDesc::new_2d(
            VK_FORMAT_R16_SFLOAT,
            inputTex.Desc().Extent2D()
        ));
        
        SimpleRenderPass::NewCompute(
            rg.AddPass("taa input prob"),
            "/shaders/taa/input_prob.hlsl"
        )
        .Read(inputTex)
        .Read(&filteredInputImg)
        .Read(&filteredInputDeviationImg)
        .Read(&reprojectedHistoryImg)
        .Read(&filteredHistoryImg)
        .Read(reprojectionMap)
        .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(&smoothVarHistoryTex)
        .Read(&velocityHistoryTex)
        .Write(&inputProbImg)
        .Constants(std::make_tuple(inputTex.Desc().ExtentInvExtent2D()))
        .Dispatch(inputProbImg.Desc().Extent());

        auto probFiltered1Img = rg.Create(*inputProbImg.Desc());
        SimpleRenderPass::NewCompute(
            rg.AddPass("taa prob filter"),
            "/shaders/taa/filter_prob.hlsl"
        )
        .Read(&inputProbImg)
        .Write(&probFiltered1Img)
        .Dispatch(probFiltered1Img.Desc().Extent());

        auto probFiltered2Img = rg.Create(*inputProbImg.Desc());
        SimpleRenderPass::NewCompute(
            rg.AddPass("taa prob filter2"),
            "/shaders/taa/filter_prob2.hlsl"
        )
        .Read(&probFiltered1Img)
        .Write(&probFiltered2Img)
        .Dispatch(probFiltered1Img.Desc().Extent());

        return probFiltered2Img;
    }();

    auto thisFrameOutputImg = rg.Create(TemporalTexDesc(outputExtent));
    SimpleRenderPass::NewCompute(rg.AddPass("taa"), "/shaders/taa/taa.hlsl")
        .Read(inputTex)
        .Read(&reprojectedHistoryImg)
        .Read(reprojectionMap)
        .Read(&closestVelocityImg)
        .Read(&velocityHistoryTex)
        .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(&smoothVarHistoryTex)
        .Read(&inputProbImg)
        .Write(&temporalOutputTex)
        .Write(&thisFrameOutputImg)
        .Write(&smoothVarOutputTex)
        .Write(&temporalVelocityOutputTex)
        .Constants(std::make_tuple(
            inputTex.Desc().ExtentInvExtent2D(),
            temporalOutputTex.Desc().ExtentInvExtent2D()
        ))
        .Dispatch(temporalOutputTex.Desc().Extent());

    return TaaOutput{
        .TemporalOut = std::move(temporalOutputTex),
        .ThisFrameOut = std::move(thisFrameOutputImg)
    };
}

}