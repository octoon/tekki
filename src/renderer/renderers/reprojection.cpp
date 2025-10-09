#include "tekki/renderer/renderers/reprojection.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace tekki::renderer::renderers {

std::shared_ptr<tekki::backend::vulkan::Image> Reprojection::CalculateReprojectionMap(
    tekki::render_graph::TemporalRenderGraph& renderGraph,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<tekki::backend::vulkan::Image>& velocityImage
) {
    //let mut output_tex = rg.create(depth.desc().format(vk::Format::R16G16B16A16_SFLOAT));
    //let mut output_tex = rg.create(depth.desc().format(vk::Format::R32G32B32A32_SFLOAT));
    auto outputTex = renderGraph.Create(
        gbufferDepth.Depth->GetDesc().WithFormat(VK_FORMAT_R16G16B16A16_SNORM)
    );

    auto prevDepth = renderGraph.GetOrCreateTemporal(
        "reprojection.prev_depth",
        gbufferDepth.Depth->GetDesc().WithFormat(VK_FORMAT_R32_SFLOAT)
            .WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
    );

    tekki::render_graph::SimpleRenderPass::NewCompute(
        renderGraph.AddPass("reprojection map"),
        "/shaders/calculate_reprojection_map.hlsl"
    )
    .ReadAspect(gbufferDepth.Depth, VK_IMAGE_ASPECT_DEPTH_BIT)
    .Read(gbufferDepth.GeometricNormal)
    .Read(prevDepth)
    .Read(velocityImage)
    .Write(outputTex)
    .Constants(outputTex->GetDesc().GetExtentInvExtent2D())
    .Dispatch(outputTex->GetDesc().GetExtent());

    tekki::render_graph::SimpleRenderPass::NewComputeRust(
        renderGraph.AddPass("copy depth"),
        "copy_depth_to_r::copy_depth_to_r_cs"
    )
    .ReadAspect(gbufferDepth.Depth, VK_IMAGE_ASPECT_DEPTH_BIT)
    .Write(prevDepth)
    .Dispatch(prevDepth->GetDesc().GetExtent());

    return outputTex;
}

} // namespace tekki::renderer::renderers