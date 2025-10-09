#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/render_graph/render_graph.h"
#include "tekki/render_graph/simple_render_pass.h"
#include "tekki/world_renderer/histogram_clipping.h"

namespace tekki::renderer::renderers {

class PostProcessRenderer {
public:
    PostProcessRenderer(std::shared_ptr<tekki::backend::vulkan::Device> device);
    
    std::shared_ptr<tekki::render_graph::ImageHandle> Render(
        tekki::render_graph::RenderGraph& renderGraph,
        std::shared_ptr<tekki::render_graph::ImageHandle> input,
        VkDescriptorSet bindlessDescriptorSet,
        float postExposureMultiplier,
        float contrast,
        HistogramClipping exposureHistogramClipping
    );

    float GetImageLog2Luminance() const { return imageLog2Luminance; }

private:
    std::shared_ptr<tekki::backend::vulkan::Buffer> histogramBuffer;
    float imageLog2Luminance;

    void CalculateLuminanceHistogram(
        tekki::render_graph::RenderGraph& renderGraph,
        std::shared_ptr<tekki::render_graph::ImageHandle> blurPyramid
    );
    
    void ReadBackHistogram(HistogramClipping exposureHistogramClipping);
};

std::shared_ptr<tekki::render_graph::ImageHandle> BlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> input
);

std::shared_ptr<tekki::render_graph::ImageHandle> ReverseBlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> inputPyramid
);

} // namespace tekki::renderer::renderers