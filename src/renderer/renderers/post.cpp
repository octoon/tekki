#include "tekki/renderer/renderers/post.h"
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <glm/glm.hpp>

namespace tekki::renderer::renderers {

constexpr size_t LUMINANCE_HISTOGRAM_BIN_COUNT = 256;
constexpr double LUMINANCE_HISTOGRAM_MIN_LOG2 = -16.0;
constexpr double LUMINANCE_HISTOGRAM_MAX_LOG2 = 16.0;

PostProcessRenderer::PostProcessRenderer(std::shared_ptr<tekki::backend::vulkan::Device> device)
    : imageLog2Luminance(0.0f) {
    try {
        auto bufferDesc = tekki::backend::vulkan::BufferDesc::NewGpuToCpu(
            sizeof(uint32_t) * LUMINANCE_HISTOGRAM_BIN_COUNT,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        );
        histogramBuffer = std::make_shared<tekki::backend::vulkan::Buffer>(device->CreateBuffer(bufferDesc, "luminance histogram"));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create PostProcessRenderer: " + std::string(e.what()));
    }
}

rg::Handle<rg::Image> PostProcessRenderer::Render(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> input,
    VkDescriptorSet bindlessDescriptorSet,
    float postExposureMultiplier,
    float contrast,
    HistogramClipping exposureHistogramClipping
) {
    // Suppress unused parameter warnings for parameters that will be used when fully implemented
    (void)bindlessDescriptorSet;
    (void)postExposureMultiplier;
    (void)contrast;

    try {
        ReadBackHistogram(exposureHistogramClipping);

        auto blurPyramid = BlurPyramid(renderGraph, input);
        // auto histogram = CalculateLuminanceHistogram(renderGraph, blurPyramid);

        auto revBlurPyramid = ReverseBlurPyramid(renderGraph, blurPyramid);

        // TODO: Implement SimpleRenderPass
        // auto outputDesc = input->WithFormat(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
        // auto output = renderGraph.Create(outputDesc);
        auto output = input; // Placeholder

        // auto extent = output->GetExtent();
        // glm::vec2 invExtent(1.0f / extent[0], 1.0f / extent[1]);

        // tekki::render_graph::SimpleRenderPass::NewCompute(renderGraph.AddPass("post combine"), "/shaders/post_combine.hlsl")
        //     .Read(input)
        //     .Read(blurPyramid)
        //     .Read(revBlurPyramid)
        //     .Read(histogram)
        //     .Write(output)
        //     .RawDescriptorSet(1, bindlessDescriptorSet)
        //     .Constants(std::make_tuple(invExtent, postExposureMultiplier, contrast))
        //     .Dispatch(extent);

        return output;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to render post process: " + std::string(e.what()));
    }
}

rg::Handle<rg::Buffer> PostProcessRenderer::CalculateLuminanceHistogram(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> blurPyramid
) {
    (void)blurPyramid;  // Will be used when fully implemented
    // TODO: Implement Simple RenderPass logic
    // For now, create an empty buffer
    auto tmpHistogramDesc = tekki::render_graph::BufferDesc::NewGpuOnly(
        sizeof(uint32_t) * LUMINANCE_HISTOGRAM_BIN_COUNT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    auto tmpHistogram = renderGraph.Create(tmpHistogramDesc);
    return tmpHistogram;
}

void PostProcessRenderer::ReadBackHistogram(HistogramClipping exposureHistogramClipping) {
    try {
        uint32_t histogram[LUMINANCE_HISTOGRAM_BIN_COUNT] = {0};
        
        auto src = histogramBuffer->GetMappedSlice();
        if (!src.empty()) {
            if (src.size() >= sizeof(histogram)) {
                std::memcpy(histogram, src.data(), sizeof(histogram));
            }
        } else {
            return;
        }

        double outlierFracLo = std::min(static_cast<double>(exposureHistogramClipping.low), 1.0);
        double outlierFracHi = std::min(static_cast<double>(exposureHistogramClipping.high), 1.0 - outlierFracLo);

        uint32_t totalEntryCount = 0;
        for (size_t i = 0; i < LUMINANCE_HISTOGRAM_BIN_COUNT; ++i) {
            totalEntryCount += histogram[i];
        }

        uint32_t rejectLoEntryCount = static_cast<uint32_t>(totalEntryCount * outlierFracLo);
        uint32_t entryCountToUse = static_cast<uint32_t>(totalEntryCount * (1.0 - outlierFracLo - outlierFracHi));

        double sum = 0.0;
        uint32_t usedCount = 0;

        uint32_t leftToReject = rejectLoEntryCount;
        uint32_t leftToUse = entryCountToUse;

        for (size_t binIdx = 0; binIdx < LUMINANCE_HISTOGRAM_BIN_COUNT; ++binIdx) {
            uint32_t count = histogram[binIdx];
            double t = (static_cast<double>(binIdx) + 0.5) / static_cast<double>(LUMINANCE_HISTOGRAM_BIN_COUNT);

            uint32_t countToUse = std::min(count - std::min(count, leftToReject), leftToUse);
            leftToReject -= std::min(count, leftToReject);
            leftToUse -= countToUse;

            sum += t * static_cast<double>(countToUse);
            usedCount += countToUse;
        }

        if (entryCountToUse != usedCount) {
            throw std::runtime_error("Histogram count mismatch");
        }

        double mean = sum / std::max(usedCount, 1u);
        imageLog2Luminance = static_cast<float>(
            LUMINANCE_HISTOGRAM_MIN_LOG2 + mean * (LUMINANCE_HISTOGRAM_MAX_LOG2 - LUMINANCE_HISTOGRAM_MIN_LOG2)
        );
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to read back histogram: " + std::string(e.what()));
    }
}

rg::Handle<rg::Image> BlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> input
) {
    (void)renderGraph;  // Will be used when fully implemented
    // TODO: Implement blur pyramid rendering with SimpleRenderPass
    // For now, just return input
    return input;
}

rg::Handle<rg::Image> ReverseBlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    rg::Handle<rg::Image> inputPyramid
) {
    (void)renderGraph;  // Will be used when fully implemented
    // TODO: Implement reverse blur pyramid with SimpleRenderPass
    // For now, just return input
    return inputPyramid;
}

} // namespace tekki::renderer::renderers