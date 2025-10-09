#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/render_graph.h"
#include "tekki/render_graph/simple_render_pass.h"

namespace tekki::renderer::renderers {

class MotionBlur {
public:
    static rg::Handle<Image> Apply(
        rg::RenderGraph& renderGraph,
        const rg::Handle<Image>& input,
        const rg::Handle<Image>& depth,
        const rg::Handle<Image>& reprojectionMap
    ) {
        const uint32_t VelocityTileSize = 16;

        auto velocityReducedX = renderGraph.Create(
            reprojectionMap.GetDescription()
                .DivideUpExtent({VelocityTileSize, 1, 1})
                .SetFormat(vk::Format::R16G16_SFLOAT)
        );

        rg::SimpleRenderPass::CreateComputeRust(
            renderGraph.AddPass("velocity reduce x"),
            "motion_blur::velocity_reduce_x"
        )
        .Read(reprojectionMap)
        .Write(velocityReducedX)
        .Dispatch(velocityReducedX.GetDescription().Extent);

        auto velocityReducedY = renderGraph.Create(
            velocityReducedX.GetDescription()
                .DivideUpExtent({1, VelocityTileSize, 1})
        );

        rg::SimpleRenderPass::CreateComputeRust(
            renderGraph.AddPass("velocity reduce y"),
            "motion_blur::velocity_reduce_y"
        )
        .Read(velocityReducedX)
        .Write(velocityReducedY)
        .Dispatch(velocityReducedX.GetDescription().Extent);

        auto velocityDilated = renderGraph.Create(velocityReducedY.GetDescription());

        rg::SimpleRenderPass::CreateComputeRust(
            renderGraph.AddPass("velocity dilate"),
            "motion_blur::velocity_dilate"
        )
        .Read(velocityReducedY)
        .Write(velocityDilated)
        .Dispatch(velocityDilated.GetDescription().Extent);

        auto output = renderGraph.Create(input.GetDescription());

        float motionBlurScale = 1.0f;

        rg::SimpleRenderPass::CreateComputeRust(
            renderGraph.AddPass("motion blur"),
            "motion_blur::motion_blur"
        )
        .Read(input)
        .Read(reprojectionMap)
        .Read(velocityDilated)
        .ReadAspect(depth, vk::ImageAspectFlagBits::DEPTH_BIT)
        .Write(output)
        .Constants(std::make_tuple(
            depth.GetDescription().GetExtentInverseExtent2D(),
            output.GetDescription().GetExtentInverseExtent2D(),
            motionBlurScale
        ))
        .Dispatch(output.GetDescription().Extent);

        return output;
    }
};

} // namespace tekki::renderer::renderers