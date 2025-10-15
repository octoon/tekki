#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/Image.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"
#include "tekki/render_graph/simple_render_pass.h"

namespace tekki::renderer::renderers {

/**
 * Trace sun shadow mask
 * @param rg The render graph
 * @param gbufferDepth The G-buffer depth
 * @param tlas The ray tracing acceleration structure
 * @param bindlessDescriptorSet The bindless descriptor set
 * @return Handle to the output image
 */
inline tekki::render_graph::Handle<tekki::backend::vulkan::Image> TraceSunShadowMask(
    tekki::render_graph::RenderGraph& rg,
    const GbufferDepth& gbufferDepth,
    const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
    VkDescriptorSet bindlessDescriptorSet
) {
    // Create output image with same dimensions as depth buffer but R8_UNORM format
    auto outputImgDesc = gbufferDepth.depth.desc;
    outputImgDesc.Format = VK_FORMAT_R8_UNORM;
    auto outputImg = rg.Create(outputImgDesc);

    // Create ray tracing pass using SimpleRenderPass
    auto passBuilder = rg.AddPass("trace shadow mask");
    auto pass = tekki::render_graph::SimpleRenderPass::NewRayTracing(
        passBuilder,
        tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/trace_sun_shadow_mask.rgen.hlsl"),
        {
            // Duplicated because `rt.hlsl` hardcodes miss index to 1
            tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl"),
            tekki::backend::vulkan::ShaderSource::CreateHlsl("/shaders/rt/shadow.rmiss.hlsl")
        },
        {} // No hit shaders
    );

    // Bind resources
    const auto& outputDesc = outputImg.desc;
    pass.ReadAspect(gbufferDepth.depth, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Read(gbufferDepth.geometric_normal)
        .Write(outputImg)
        .RawDescriptorSet(1, bindlessDescriptorSet)
        .TraceRays(tlas, glm::u32vec3(outputDesc.Extent.x, outputDesc.Extent.y, outputDesc.Extent.z));

    return outputImg;
}

} // namespace tekki::renderer::renderers