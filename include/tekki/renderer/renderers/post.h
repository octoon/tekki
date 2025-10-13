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
#include "tekki/render_graph/graph.h"
#include "tekki/renderer/histogram_clipping.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

class PostProcessRenderer {
public:
    PostProcessRenderer(std::shared_ptr<tekki::backend::vulkan::Device> device);

    rg::Handle<rg::Image> Render(
        tekki::render_graph::RenderGraph& renderGraph,
        rg::Handle<rg::Image> input,
        VkDescriptorSet bindlessDescriptorSet,
        float postExposureMultiplier,
        float contrast,
        HistogramClipping exposureHistogramClipping
    );

    float GetImageLog2Luminance() const { return imageLog2Luminance; }

private:
    std::shared_ptr<tekki::backend::vulkan::Buffer> histogramBuffer;
    float imageLog2Luminance;

    rg::Handle<rg::Buffer> CalculateLuminanceHistogram(
        tekki::render_graph::RenderGraph& renderGraph,
        rg::Handle<rg::Image> blurPyramid
    );

    void ReadBackHistogram(HistogramClipping exposureHistogramClipping);
};

rg::Handle<rg::Image> BlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> input
);

rg::Handle<rg::Image> ReverseBlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> inputPyramid
);

} // namespace tekki::renderer::renderers