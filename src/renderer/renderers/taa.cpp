#include "tekki/renderer/renderers/taa.h"
#include <glm/glm.hpp>
#include <array>
#include <stdexcept>
#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/simple_render_pass.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

TaaRenderer::TaaRenderer()
    : temporalTex("taa")
    , temporalVelocityTex("taa.velocity")
    , temporalSmoothVarTex("taa.smooth_var")
    , CurrentSupersampleOffset(0.0f, 0.0f) {
}

rg::ImageDesc TaaRenderer::TemporalTexDesc(const std::array<uint32_t, 2>& extent) {
    return rg::ImageDesc::New2d(VK_FORMAT_R16G16B16A16_SFLOAT, {extent[0], extent[1]})
        .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
}

TaaOutput TaaRenderer::Render(
    rg::TemporalRenderGraph& rg,
    const rg::Handle<tekki::backend::vulkan::Image>& inputTex,
    const rg::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
    const rg::Handle<tekki::backend::vulkan::Image>& depthTex,
    const std::array<uint32_t, 2>& outputExtent
) {
    auto [temporalOutputTex, historyTex] = temporalTex.GetOutputAndHistory(rg, TemporalTexDesc(outputExtent));

    auto [temporalVelocityOutputTex, velocityHistoryTex] = temporalVelocityTex.GetOutputAndHistory(
        rg,
        rg::ImageDesc::New2d(VK_FORMAT_R16G16_SFLOAT, {outputExtent[0], outputExtent[1]})
            .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    auto reprojectedHistoryImg = rg.Create(TemporalTexDesc(outputExtent));
    auto closestVelocityImg = rg.Create(rg::ImageDesc::New2d(VK_FORMAT_R16G16_SFLOAT, {outputExtent[0], outputExtent[1]}));

    {
        auto pass = rg.AddPass("reproject taa");
        auto renderPass = rg::SimpleRenderPass::NewCompute(
            pass,
            "/shaders/taa/reproject_history.hlsl"
        );
        renderPass
            .Read(historyTex)
            .Read(reprojectionMap)
            .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Write(reprojectedHistoryImg)
            .Write(closestVelocityImg)
            .Constants(std::make_tuple(
                glm::vec4(inputTex.desc.Extent.x, inputTex.desc.Extent.y,
                         1.0f/inputTex.desc.Extent.x, 1.0f/inputTex.desc.Extent.y),
                glm::vec4(reprojectedHistoryImg.desc.Extent.x, reprojectedHistoryImg.desc.Extent.y,
                         1.0f/reprojectedHistoryImg.desc.Extent.x, 1.0f/reprojectedHistoryImg.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec2(reprojectedHistoryImg.desc.Extent.x, reprojectedHistoryImg.desc.Extent.y));
    }

    auto [smoothVarOutputTex, smoothVarHistoryTex] = temporalSmoothVarTex.GetOutputAndHistory(
        rg,
        rg::ImageDesc::New2d(VK_FORMAT_R16G16B16A16_SFLOAT, {outputExtent[0], outputExtent[1]})
            .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    auto filteredInputImg = rg.Create(rg::ImageDesc::New2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        {inputTex.desc.Extent.x, inputTex.desc.Extent.y}
    ));

    auto filteredInputDeviationImg = rg.Create(rg::ImageDesc::New2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        {inputTex.desc.Extent.x, inputTex.desc.Extent.y}
    ));

    {
        auto pass = rg.AddPass("taa filter input");
        auto renderPass = rg::SimpleRenderPass::NewCompute(
            pass,
            "/shaders/taa/filter_input.hlsl"
        );
        renderPass
            .Read(inputTex)
            .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Write(filteredInputImg)
            .Write(filteredInputDeviationImg)
            .Dispatch(glm::u32vec2(filteredInputImg.desc.Extent.x, filteredInputImg.desc.Extent.y));
    }

    auto filteredHistoryImg = rg.Create(rg::ImageDesc::New2d(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        {filteredInputImg.desc.Extent.x, filteredInputImg.desc.Extent.y}
    ));

    {
        auto pass = rg.AddPass("taa filter history");
        auto renderPass = rg::SimpleRenderPass::NewCompute(
            pass,
            "/shaders/taa/filter_history.hlsl"
        );
        renderPass
            .Read(reprojectedHistoryImg)
            .Write(filteredHistoryImg)
            .Constants(std::make_tuple(
                glm::vec4(reprojectedHistoryImg.desc.Extent.x, reprojectedHistoryImg.desc.Extent.y,
                         1.0f/reprojectedHistoryImg.desc.Extent.x, 1.0f/reprojectedHistoryImg.desc.Extent.y),
                glm::vec4(inputTex.desc.Extent.x, inputTex.desc.Extent.y,
                         1.0f/inputTex.desc.Extent.x, 1.0f/inputTex.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec2(filteredHistoryImg.desc.Extent.x, filteredHistoryImg.desc.Extent.y));
    }

    auto inputProbImg = [&]() -> rg::Handle<tekki::backend::vulkan::Image> {
        auto inputProbImg = rg.Create(rg::ImageDesc::New2d(
            VK_FORMAT_R16_SFLOAT,
            {inputTex.desc.Extent.x, inputTex.desc.Extent.y}
        ));

        {
            auto pass = rg.AddPass("taa input prob");
            auto renderPass = rg::SimpleRenderPass::NewCompute(
                pass,
                "/shaders/taa/input_prob.hlsl"
            );
            renderPass
                .Read(inputTex)
                .Read(filteredInputImg)
                .Read(filteredInputDeviationImg)
                .Read(reprojectedHistoryImg)
                .Read(filteredHistoryImg)
                .Read(reprojectionMap)
                .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
                .Read(smoothVarHistoryTex)
                .Read(velocityHistoryTex)
                .Write(inputProbImg)
                .Constants(std::make_tuple(
                    glm::vec4(inputTex.desc.Extent.x, inputTex.desc.Extent.y,
                             1.0f/inputTex.desc.Extent.x, 1.0f/inputTex.desc.Extent.y)
                ))
                .Dispatch(glm::u32vec2(inputProbImg.desc.Extent.x, inputProbImg.desc.Extent.y));
        }

        auto probFiltered1Img = rg.Create(rg::ImageDesc::New2d(
            VK_FORMAT_R16_SFLOAT,
            {inputProbImg.desc.Extent.x, inputProbImg.desc.Extent.y}
        ));
        {
            auto pass = rg.AddPass("taa prob filter");
            auto renderPass = rg::SimpleRenderPass::NewCompute(
                pass,
                "/shaders/taa/filter_prob.hlsl"
            );
            renderPass
                .Read(inputProbImg)
                .Write(probFiltered1Img)
                .Dispatch(glm::u32vec2(probFiltered1Img.desc.Extent.x, probFiltered1Img.desc.Extent.y));
        }

        auto probFiltered2Img = rg.Create(rg::ImageDesc::New2d(
            VK_FORMAT_R16_SFLOAT,
            {inputProbImg.desc.Extent.x, inputProbImg.desc.Extent.y}
        ));
        {
            auto pass = rg.AddPass("taa prob filter2");
            auto renderPass = rg::SimpleRenderPass::NewCompute(
                pass,
                "/shaders/taa/filter_prob2.hlsl"
            );
            renderPass
                .Read(probFiltered1Img)
                .Write(probFiltered2Img)
                .Dispatch(glm::u32vec2(probFiltered1Img.desc.Extent.x, probFiltered1Img.desc.Extent.y));
        }

        return probFiltered2Img;
    }();

    auto thisFrameOutputImg = rg.Create(TemporalTexDesc(outputExtent));
    {
        auto pass = rg.AddPass("taa");
        auto renderPass = rg::SimpleRenderPass::NewCompute(
            pass,
            "/shaders/taa/taa.hlsl"
        );
        renderPass
            .Read(inputTex)
            .Read(reprojectedHistoryImg)
            .Read(reprojectionMap)
            .Read(closestVelocityImg)
            .Read(velocityHistoryTex)
            .ReadAspect(depthTex, VK_IMAGE_ASPECT_DEPTH_BIT)
            .Read(smoothVarHistoryTex)
            .Read(inputProbImg)
            .Write(temporalOutputTex)
            .Write(thisFrameOutputImg)
            .Write(smoothVarOutputTex)
            .Write(temporalVelocityOutputTex)
            .Constants(std::make_tuple(
                glm::vec4(inputTex.desc.Extent.x, inputTex.desc.Extent.y,
                         1.0f/inputTex.desc.Extent.x, 1.0f/inputTex.desc.Extent.y),
                glm::vec4(temporalOutputTex.desc.Extent.x, temporalOutputTex.desc.Extent.y,
                         1.0f/temporalOutputTex.desc.Extent.x, 1.0f/temporalOutputTex.desc.Extent.y)
            ))
            .Dispatch(glm::u32vec2(temporalOutputTex.desc.Extent.x, temporalOutputTex.desc.Extent.y));
    }

    return TaaOutput{
        .TemporalOut = std::move(temporalOutputTex),
        .ThisFrameOut = std::move(thisFrameOutputImg)
    };
}

}