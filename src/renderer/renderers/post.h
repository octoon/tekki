#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class PostProcessRenderer {
public:
    PostProcessRenderer();

    render_graph::Handle<vulkan::Image> apply_post_processing(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth,
        const std::array<uint32_t, 2>& output_extent,
        float exposure_multiplier,
        float contrast
    );

    render_graph::Handle<vulkan::Image> blur_pyramid(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input
    );

    render_graph::Handle<vulkan::Image> rev_blur_pyramid(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& in_pyramid
    );

    void compute_exposure_histogram(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const HistogramClipping& clipping
    );

private:
    // Post-processing state
    std::shared_ptr<vulkan::Buffer> exposure_histogram_buffer_;
};

} // namespace tekki::renderer