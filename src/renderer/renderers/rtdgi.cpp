#include "../../include/tekki/renderer/renderers/rtdgi.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

RtdgiRenderer::RtdgiRenderer() : rtdgi_tex_("rtdgi") {}

render_graph::ReadOnlyHandle<vulkan::Image>
RtdgiRenderer::render(render_graph::TemporalRenderGraph& rg, const GbufferDepth& gbuffer_depth,
                     const render_graph::Handle<vulkan::Image>& reprojection_map,
                     const render_graph::Handle<vulkan::Image>& prev_radiance,
                     vk::DescriptorSet bindless_descriptor_set)
{
    auto [rtdgi_output, rtdgi_history] = rtdgi_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(rg.resource_desc(gbuffer_depth.geometric_normal).extent[0],
                                      rg.resource_desc(gbuffer_depth.geometric_normal).extent[1],
                                      vk::Format::eR16G16B16A16Sfloat));

    // RTDGI main computation pass
    rg.add_pass("rtdgi_compute",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({gbuffer_depth.geometric_normal, gbuffer_depth.gbuffer, gbuffer_depth.depth, reprojection_map, prev_radiance});
                    pb.writes({rtdgi_output});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement RTDGI compute shader
                            // This would involve:
                            // 1. Binding RTDGI compute pipeline
                            // 2. Setting descriptor sets (G-buffer, depth, reprojection, history)
                            // 3. Dispatching compute shader
                            // 4. Ray-traced diffuse global illumination computation
                            // 5. Temporal accumulation with reprojection

                            // For now, clear the output
                            vk::ClearColorValue clear_color{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            };
                            cmd.clearColorImage(pb.get_image(rtdgi_output), vk::ImageLayout::eGeneral, &clear_color, 1, &range);
                        });
                });

    // RTDGI filtering pass (denoising)
    filter_rtdgi(rg, rtdgi_output, gbuffer_depth, reprojection_map, rtdgi_tex_);

    return rtdgi_output;
}

void RtdgiRenderer::filter_rtdgi(render_graph::TemporalRenderGraph& rg, const render_graph::Handle<vulkan::Image>& input,
                               const GbufferDepth& gbuffer_depth,
                               const render_graph::Handle<vulkan::Image>& reprojection_map,
                               PingPongTemporalResource& temporal_tex)
{
    auto [filtered_output, filtered_history] = temporal_tex.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(rg.resource_desc(input).extent[0],
                                      rg.resource_desc(input).extent[1],
                                      vk::Format::eR16G16B16A16Sfloat));

    // RTDGI filtering pass (bilateral filtering)
    rg.add_pass("rtdgi_filter",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input, gbuffer_depth.geometric_normal, gbuffer_depth.depth});
                    pb.writes({filtered_output});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement RTDGI bilateral filtering
                            // This would involve:
                            // 1. Binding bilateral filter compute pipeline
                            // 2. Setting descriptor sets (input, G-buffer, depth)
                            // 3. Dispatching compute shader
                            // 4. Edge-aware filtering to reduce noise

                            // For now, copy input to output
                            vk::ImageCopy copy_region{
                                .srcSubresource = {
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .mipLevel = 0,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                },
                                .dstSubresource = {
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .mipLevel = 0,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                },
                                .extent = {
                                    .width = rg.resource_desc(input).extent[0],
                                    .height = rg.resource_desc(input).extent[1],
                                    .depth = 1
                                }
                            };
                            cmd.copyImage(pb.get_image(input), vk::ImageLayout::eGeneral,
                                         pb.get_image(filtered_output), vk::ImageLayout::eGeneral,
                                         1, &copy_region);
                        });
                });
}

} // namespace tekki::renderer