#include "../../include/tekki/renderer/half_res.h"
#include "../../include/tekki/backend/vulkan/shader.h"

namespace tekki::renderer {

render_graph::Handle<vulkan::Image> extract_half_res_gbuffer_view_normal_rgba8(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& gbuffer)
{
    const auto gbuffer_desc = rg.resource_desc(gbuffer);

    // Create half-resolution output texture
    auto output_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(
            gbuffer_desc.extent[0] / 2,
            gbuffer_desc.extent[1] / 2,
            vk::Format::eR8G8B8A8Snorm
        ),
        "half_res_view_normal"
    );

    // Extract half-resolution view normal pass
    rg.add_pass("extract_view_normal_half",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({gbuffer});
                    pb.writes({output_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement extract_half_res_gbuffer_view_normal_rgba8 compute shader
                        // This would involve:
                        // 1. Binding compute pipeline for half-res extraction
                        // 2. Reading from full-resolution G-buffer
                        // 3. Extracting and packing view normal to RGBA8_SNORM
                        // 4. Writing to half-resolution output

                        const auto output_desc = rg.resource_desc(output_tex);
                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1
                        };
                        cmd.clearColorImage(pb.get_image(output_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return output_tex;
}

render_graph::Handle<vulkan::Image> extract_half_res_depth(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& depth)
{
    const auto depth_desc = rg.resource_desc(depth);

    // Create half-resolution depth output texture
    auto output_tex = rg.create_image(
        vulkan::ImageDesc::new_2d(
            depth_desc.extent[0] / 2,
            depth_desc.extent[1] / 2,
            vk::Format::eR32Sfloat
        ),
        "half_res_depth"
    );

    // Extract half-resolution depth pass
    rg.add_pass("extract_half_depth",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({depth});
                    pb.writes({output_tex});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement extract_half_res_depth compute shader
                        // This would involve:
                        // 1. Binding compute pipeline for half-res depth extraction
                        // 2. Reading from full-resolution depth buffer (depth aspect)
                        // 3. Downsampling depth values appropriately (min/max/average)
                        // 4. Writing to half-resolution R32_SFLOAT output

                        const auto output_desc = rg.resource_desc(output_tex);
                        vk::ClearColorValue clear{std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1
                        };
                        cmd.clearColorImage(pb.get_image(output_tex), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return output_tex;
}

} // namespace tekki::renderer