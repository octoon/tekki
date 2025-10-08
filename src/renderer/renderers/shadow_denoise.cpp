#include "../../include/tekki/renderer/renderers/shadow_denoise.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

ShadowDenoiseRenderer::ShadowDenoiseRenderer() {}

render_graph::Handle<vulkan::Image> ShadowDenoiseRenderer::denoise_shadows(render_graph::RenderGraph& rg,
                                                                            const render_graph::Handle<vulkan::Image>& shadow_map,
                                                                            const GbufferDepth& gbuffer_depth,
                                                                            const std::array<uint32_t, 2>& output_extent)
{
    auto denoised_shadows = rg.create_image(
        vulkan::ImageDesc::new_2d(output_extent[0], output_extent[1], vk::Format::eR16G16B16A16Sfloat), "denoised_shadows");

    // Shadow denoising pass
    rg.add_pass("shadow_denoise",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({shadow_map, gbuffer_depth.geometric_normal, gbuffer_depth.depth});
                    pb.writes({denoised_shadows});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement shadow denoising compute shader
                            // This would involve:
                            // 1. Binding shadow denoising compute pipeline
                            // 2. Setting descriptor sets (shadow map, G-buffer, depth)
                            // 3. Dispatching compute shader
                            // 4. Bilateral filtering of shadow maps
                            // 5. Edge-aware denoising
                            // 6. Temporal accumulation for shadow stability

                            // For now, copy shadow map to output
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
                                    .width = output_extent[0],
                                    .height = output_extent[1],
                                    .depth = 1
                                }
                            };
                            cmd.copyImage(pb.get_image(shadow_map), vk::ImageLayout::eGeneral,
                                         pb.get_image(denoised_shadows), vk::ImageLayout::eGeneral,
                                         1, &copy_region);
                        });
                });

    return denoised_shadows;
}

} // namespace tekki::renderer