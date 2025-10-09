#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class PostProcessRenderer {
public:
    PostProcessRenderer();

    // Main render method matching original Rust implementation
    render_graph::Handle<vulkan::Image> render(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        vk::DescriptorSet bindless_descriptor_set,
        float post_exposure_mult,
        float contrast,
        const HistogramClipping& exposure_histogram_clipping
    );

    // Legacy post-processing method for compatibility
    render_graph::Handle<vulkan::Image> apply_post_processing(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth,
        const std::array<uint32_t, 2>& output_extent,
        float exposure_multiplier,
        float contrast
    );

    // Blur pyramid generation
    render_graph::Handle<vulkan::Image> blur_pyramid(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input
    );

    // Reverse blur pyramid reconstruction
    render_graph::Handle<vulkan::Image> rev_blur_pyramid(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& in_pyramid
    );

    // Exposure histogram computation
    void compute_exposure_histogram(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const HistogramClipping& clipping
    );

    // Get current image luminance
    float get_image_log2_luminance() const;

private:
    // Post-processing state
    std::shared_ptr<vulkan::Buffer> exposure_histogram_buffer_;
    float image_log2_lum_;

    // Helper methods
    render_graph::Handle<vulkan::Buffer> calculate_luminance_histogram(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& blur_pyramid
    );

    void read_back_histogram(const HistogramClipping& exposure_histogram_clipping);
};

} // namespace tekki::renderer