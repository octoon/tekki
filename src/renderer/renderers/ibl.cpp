#include "../../include/tekki/renderer/renderers/ibl.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

IblRenderer::IblRenderer() {}

render_graph::Handle<vulkan::Image> IblRenderer::render(render_graph::RenderGraph& rg, const GbufferDepth& gbuffer_depth,
                                                         const std::array<uint32_t, 2>& output_extent)
{
    auto ibl_output = rg.create_image(
        vulkan::ImageDesc::new_2d(output_extent[0], output_extent[1], vk::Format::eR16G16B16A16Sfloat), "ibl_output");

    // IBL computation pass
    rg.add_pass("ibl_compute",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({gbuffer_depth.geometric_normal, gbuffer_depth.gbuffer, gbuffer_depth.depth});
                    pb.writes({ibl_output});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement IBL compute shader
                            // This would involve:
                            // 1. Binding IBL compute pipeline
                            // 2. Setting descriptor sets (G-buffer, depth, environment map)
                            // 3. Dispatching compute shader
                            // 4. Image-based lighting computation
                            // 5. Diffuse and specular environment lighting
                            // 6. Importance sampling for environment map

                            // For now, clear the output
                            vk::ClearColorValue clear_color{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            };
                            cmd.clearColorImage(pb.get_image(ibl_output), vk::ImageLayout::eGeneral, &clear_color, 1, &range);
                        });
                });

    return ibl_output;
}

} // namespace tekki::renderer