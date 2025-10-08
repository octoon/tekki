#include "../../include/tekki/renderer/renderers/taa.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

TaaRenderer::TaaRenderer() : taa_tex_("taa") {}

TaaRenderer::TaaOutput TaaRenderer::render(
    render_graph::TemporalRenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input_tex,
    const render_graph::Handle<vulkan::Image>& reprojection_map,
    const render_graph::Handle<vulkan::Image>& depth_tex,
    const std::array<uint32_t, 2>& output_extent)
{
    auto [temporal_out, temporal_history] = temporal_tex_.get_output_and_history(
        rg, temporal_tex_desc(output_extent));

    // TAA main pass
    rg.add_pass("taa",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input_tex, reprojection_map, depth_tex, temporal_history});
                    pb.writes({temporal_out});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement TAA compute shader
                            // This would involve:
                            // 1. Binding TAA compute pipeline
                            // 2. Setting descriptor sets (current frame, reprojection, depth, history)
                            // 3. Dispatching compute shader
                            // 4. Motion vector reprojection
                            // 5. History accumulation with neighborhood clamping
                            // 6. Anti-flickering techniques

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
                                    .width = output_extent[0],
                                    .height = output_extent[1],
                                    .depth = 1
                                }
                            };
                            cmd.copyImage(pb.get_image(input_tex), vk::ImageLayout::eGeneral,
                                         pb.get_image(temporal_out), vk::ImageLayout::eGeneral,
                                         1, &copy_region);
                        });
                });

    return TaaOutput{
        .temporal_out = temporal_out,
        .this_frame_out = input_tex
    };
}

vulkan::ImageDesc TaaRenderer::temporal_tex_desc(const std::array<uint32_t, 2>& extent)
{
    return vulkan::ImageDesc::new_2d(extent[0], extent[1], vk::Format::eR16G16B16A16Sfloat);
}

} // namespace tekki::renderer