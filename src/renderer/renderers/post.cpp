#include "../../include/tekki/renderer/renderers/post.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"

namespace tekki::renderer
{

// Luminance histogram constants from original Rust implementation
constexpr uint32_t LUMINANCE_HISTOGRAM_BIN_COUNT = 256;
constexpr double LUMINANCE_HISTOGRAM_MIN_LOG2 = -16.0;
constexpr double LUMINANCE_HISTOGRAM_MAX_LOG2 = 16.0;

PostProcessRenderer::PostProcessRenderer() : image_log2_lum_(0.0f)
{
    // Initialize post-processing resources
    // TODO: Create exposure histogram buffer when device is available
}

render_graph::Handle<vulkan::Image> PostProcessRenderer::render(
    render_graph::RenderGraph& rg,
    const render_graph::Handle<vulkan::Image>& input,
    vk::DescriptorSet bindless_descriptor_set,
    float post_exposure_mult,
    float contrast,
    const HistogramClipping& exposure_histogram_clipping)
{
    // Read back histogram (simulated for now)
    read_back_histogram(exposure_histogram_clipping);

    // Generate blur pyramid
    auto blur_pyramid_result = blur_pyramid(rg, input);

    // Calculate luminance histogram
    auto histogram_buffer = calculate_luminance_histogram(rg, blur_pyramid_result);

    // Generate reverse blur pyramid
    auto rev_blur_pyramid_result = rev_blur_pyramid(rg, blur_pyramid_result);

    // Create final output with appropriate format
    const auto input_desc = rg.resource_desc(input);
    auto output = rg.create_image(
        vulkan::ImageDesc::new_2d(input_desc.extent[0], input_desc.extent[1], vk::Format::eB10G11R11UfloatPack32),
        "post_output");

    // Post combine pass - combines all effects
    rg.add_pass("post_combine",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input, blur_pyramid_result, rev_blur_pyramid_result, histogram_buffer});
                    pb.writes({output});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement post combine compute shader
                        // This would involve:
                        // 1. Tone mapping with exposure multiplier
                        // 2. Color grading and contrast adjustment
                        // 3. Bloom effect from blur pyramid
                        // 4. Dynamic exposure based on histogram
                        // 5. Output format conversion

                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(output), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return output;
}

render_graph::Handle<vulkan::Image> PostProcessRenderer::blur_pyramid(render_graph::RenderGraph& rg,
                                                                      const render_graph::Handle<vulkan::Image>& input)
{
    const auto input_desc = rg.resource_desc(input);

    // Calculate mip levels for pyramid, skipping bottom mips as in original
    const uint32_t skip_n_bottom_mips = 1;
    uint32_t mip_levels = std::max(1u,
        static_cast<uint32_t>(std::floor(std::log2(std::max(input_desc.extent[0], input_desc.extent[1])))) - skip_n_bottom_mips);

    // Create blur pyramid with multiple mip levels
    auto pyramid_output = rg.create_image(
        vulkan::ImageDesc::new_2d(input_desc.extent[0] / 2, input_desc.extent[1] / 2, vk::Format::eB10G11R11UfloatPack32)
            .mip_levels(mip_levels),
        "blur_pyramid");

    // Initial blur pass - downsample and blur input to first mip level
    rg.add_pass("blur_0",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input});
                    pb.writes({pyramid_output}); // Writes to mip 0
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement initial blur compute shader
                        // This downsamples the input to half resolution with blur

                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(pyramid_output), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    // Generate additional mip levels
    for (uint32_t target_mip = 1; target_mip < mip_levels; ++target_mip) {
        const uint32_t downsample_amount = 1u << target_mip;
        const std::array<uint32_t, 3> mip_extent = {
            input_desc.extent[0] / (2 * downsample_amount),
            input_desc.extent[1] / (2 * downsample_amount),
            1
        };

        rg.add_pass("blur_" + std::to_string(target_mip),
                    [&, target_mip, mip_extent](render_graph::PassBuilder& pb)
                    {
                        pb.reads({pyramid_output}); // Reads from mip target_mip-1
                        pb.writes({pyramid_output}); // Writes to mip target_mip
                        pb.executes([&](vk::CommandBuffer cmd) {
                            // TODO: Implement mip level blur compute shader
                            // This reads from the previous mip level and generates the next

                            vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = target_mip, .levelCount = 1,
                                .baseArrayLayer = 0, .layerCount = 1};
                            cmd.clearColorImage(pb.get_image(pyramid_output), vk::ImageLayout::eGeneral, &clear, 1, &range);
                        });
                    });
    }

    return pyramid_output;
}

render_graph::Handle<vulkan::Image>
PostProcessRenderer::rev_blur_pyramid(render_graph::RenderGraph& rg,
                                      const render_graph::Handle<vulkan::Image>& in_pyramid)
{
    const auto pyramid_desc = rg.resource_desc(in_pyramid);

    // Create reverse blur pyramid output with same descriptor
    auto rev_pyramid_output = rg.create_image(
        vulkan::ImageDesc::new_2d(pyramid_desc.extent[0], pyramid_desc.extent[1], pyramid_desc.format)
            .mip_levels(pyramid_desc.mip_levels),
        "rev_blur_pyramid");

    // Process mip levels in reverse order (from highest to lowest)
    for (uint32_t target_mip = pyramid_desc.mip_levels - 1; target_mip > 0; --target_mip) {
        const uint32_t downsample_amount = 1u << target_mip;
        const std::array<uint32_t, 3> output_extent = {
            pyramid_desc.extent[0] / downsample_amount,
            pyramid_desc.extent[1] / downsample_amount,
            1
        };

        const uint32_t src_mip = target_mip + 1;
        const float self_weight = (src_mip == pyramid_desc.mip_levels) ? 0.0f : 0.5f;

        rg.add_pass("rev_blur_" + std::to_string(target_mip),
                    [&, target_mip, src_mip, output_extent, self_weight](render_graph::PassBuilder& pb)
                    {
                        pb.reads({in_pyramid, rev_pyramid_output}); // Read from pyramid and previous output
                        pb.writes({rev_pyramid_output}); // Write to current mip level
                        pb.executes([&](vk::CommandBuffer cmd) {
                            // TODO: Implement reverse blur compute shader
                            // This upsamples and combines with the current mip level
                            // Constants: output_extent[0], output_extent[1], self_weight

                            vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = target_mip, .levelCount = 1,
                                .baseArrayLayer = 0, .layerCount = 1};
                            cmd.clearColorImage(pb.get_image(rev_pyramid_output), vk::ImageLayout::eGeneral, &clear, 1, &range);
                        });
                    });
    }

    return rev_pyramid_output;
}

render_graph::Handle<vulkan::Buffer>
PostProcessRenderer::calculate_luminance_histogram(render_graph::RenderGraph& rg,
                                                   const render_graph::Handle<vulkan::Image>& blur_pyramid)
{
    // Create temporary histogram buffer
    auto tmp_histogram = rg.create_buffer(
        vulkan::BufferDesc::new_gpu_only(
            sizeof(uint32_t) * LUMINANCE_HISTOGRAM_BIN_COUNT,
            vk::BufferUsageFlags::eStorageBuffer),
        "tmp_histogram");

    const auto pyramid_desc = rg.resource_desc(blur_pyramid);

    // Start with input downsampled to a fairly consistent size
    const uint32_t input_mip_level = std::max(0u, pyramid_desc.mip_levels - 7);
    const uint32_t mip_scale = 1u << input_mip_level;
    const std::array<uint32_t, 3> mip_extent = {
        (pyramid_desc.extent[0] + mip_scale - 1) / mip_scale,  // Div up
        (pyramid_desc.extent[1] + mip_scale - 1) / mip_scale,
        1
    };

    // Clear histogram
    rg.add_pass("clear_histogram",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.writes({tmp_histogram});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement histogram clear compute shader
                        // Dispatch [LUMINANCE_HISTOGRAM_BIN_COUNT, 1, 1]
                    });
                });

    // Calculate histogram
    rg.add_pass("calculate_histogram",
                [&, input_mip_level, mip_extent](render_graph::PassBuilder& pb)
                {
                    pb.reads({blur_pyramid}); // Read from specific mip level
                    pb.writes({tmp_histogram});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement histogram calculation compute shader
                        // Constants: mip_extent[0], mip_extent[1]
                        // Reads from blur_pyramid mip level input_mip_level
                        // Dispatch mip_extent
                    });
                });

    // Copy to persistent histogram buffer (if we had one)
    // For now, return the temporary buffer
    return tmp_histogram;
}

void PostProcessRenderer::read_back_histogram(const HistogramClipping& exposure_histogram_clipping)
{
    // Simulate histogram readback
    // In the real implementation, this would:
    // 1. Map the histogram buffer
    // 2. Read the histogram data
    // 3. Calculate the mean luminance with clipping
    // 4. Update image_log2_lum_

    // For now, just use default values
    const double mean_t = 0.5; // Middle of histogram range
    image_log2_lum_ = static_cast<float>(
        LUMINANCE_HISTOGRAM_MIN_LOG2 +
        mean_t * (LUMINANCE_HISTOGRAM_MAX_LOG2 - LUMINANCE_HISTOGRAM_MIN_LOG2));
}

render_graph::Handle<vulkan::Image> PostProcessRenderer::apply_post_processing(
    render_graph::RenderGraph& rg, const render_graph::Handle<vulkan::Image>& input,
    const render_graph::Handle<vulkan::Image>& depth, const std::array<uint32_t, 2>& output_extent,
    float exposure_multiplier, float contrast)
{
    // Simple post-processing without full histogram support
    auto post_output = rg.create_image(
        vulkan::ImageDesc::new_2d(output_extent[0], output_extent[1], vk::Format::eR8G8B8A8Unorm),
        "post_output");

    rg.add_pass("simple_post_processing",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input, depth});
                    pb.writes({post_output});
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement simple post-processing compute shader
                        // This would apply basic tone mapping and color correction

                        vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
                        vk::ImageSubresourceRange range{
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0, .levelCount = 1,
                            .baseArrayLayer = 0, .layerCount = 1};
                        cmd.clearColorImage(pb.get_image(post_output), vk::ImageLayout::eGeneral, &clear, 1, &range);
                    });
                });

    return post_output;
}

void PostProcessRenderer::compute_exposure_histogram(render_graph::RenderGraph& rg,
                                                     const render_graph::Handle<vulkan::Image>& input,
                                                     const HistogramClipping& clipping)
{
    // Exposure histogram computation pass
    rg.add_pass("exposure_histogram",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input});
                    pb.writes({}); // Histogram buffer would be written here
                    pb.executes([&](vk::CommandBuffer cmd) {
                        // TODO: Implement exposure histogram compute shader
                        // This would compute luminance histogram for dynamic exposure
                    });
                });
}

float PostProcessRenderer::get_image_log2_luminance() const
{
    return image_log2_lum_;
}

} // namespace tekki::renderer