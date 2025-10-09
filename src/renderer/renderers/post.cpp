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
        histogramBuffer = device->CreateBuffer(bufferDesc, "luminance histogram", nullptr);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create PostProcessRenderer: " + std::string(e.what()));
    }
}

std::shared_ptr<tekki::render_graph::ImageHandle> PostProcessRenderer::Render(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> input,
    VkDescriptorSet bindlessDescriptorSet,
    float postExposureMultiplier,
    float contrast,
    HistogramClipping exposureHistogramClipping
) {
    try {
        ReadBackHistogram(exposureHistogramClipping);

        auto blurPyramid = BlurPyramid(renderGraph, input);
        auto histogram = CalculateLuminanceHistogram(renderGraph, blurPyramid);

        auto revBlurPyramid = ReverseBlurPyramid(renderGraph, blurPyramid);

        auto outputDesc = input->GetDesc().Format(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
        auto output = renderGraph.Create(outputDesc);

        auto extent = output->GetDesc().GetExtent();
        glm::vec2 invExtent(1.0f / extent[0], 1.0f / extent[1]);

        tekki::render_graph::SimpleRenderPass::NewCompute(renderGraph.AddPass("post combine"), "/shaders/post_combine.hlsl")
            .Read(input)
            .Read(blurPyramid)
            .Read(revBlurPyramid)
            .Read(histogram)
            .Write(output)
            .RawDescriptorSet(1, bindlessDescriptorSet)
            .Constants(std::make_tuple(invExtent, postExposureMultiplier, contrast))
            .Dispatch(extent);

        return output;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to render post process: " + std::string(e.what()));
    }
}

void PostProcessRenderer::CalculateLuminanceHistogram(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> blurPyramid
) {
    try {
        auto tmpHistogramDesc = tekki::backend::vulkan::BufferDesc::NewGpuOnly(
            sizeof(uint32_t) * LUMINANCE_HISTOGRAM_BIN_COUNT,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        );
        auto tmpHistogram = renderGraph.Create(tmpHistogramDesc);

        uint32_t inputMipLevel = std::max(static_cast<int32_t>(blurPyramid->GetDesc().GetMipLevels()) - 7, 0);
        auto mipExtent = blurPyramid->GetDesc().DivUpExtent({1u << inputMipLevel, 1u << inputMipLevel, 1u}).GetExtent();

        tekki::render_graph::SimpleRenderPass::NewCompute(renderGraph.AddPass("_clear histogram"), "/shaders/post/luminance_histogram_clear.hlsl")
            .Write(tmpHistogram)
            .Dispatch({static_cast<uint32_t>(LUMINANCE_HISTOGRAM_BIN_COUNT), 1, 1});

        tekki::render_graph::SimpleRenderPass::NewCompute(renderGraph.AddPass("calculate histogram"), "/shaders/post/luminance_histogram_calculate.hlsl")
            .ReadView(blurPyramid, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(inputMipLevel)
                .LevelCount(1))
            .Write(tmpHistogram)
            .Constants(std::make_tuple(mipExtent[0], mipExtent[1]))
            .Dispatch(mipExtent);

        auto dstHistogram = renderGraph.Import(histogramBuffer, vk_sync::AccessType::Nothing);
        tekki::render_graph::SimpleRenderPass::NewCompute(renderGraph.AddPass("_copy histogram"), "/shaders/post/luminance_histogram_copy.hlsl")
            .Read(tmpHistogram)
            .Write(dstHistogram)
            .Dispatch({static_cast<uint32_t>(LUMINANCE_HISTOGRAM_BIN_COUNT), 1, 1});

        return tmpHistogram;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to calculate luminance histogram: " + std::string(e.what()));
    }
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

std::shared_ptr<tekki::render_graph::ImageHandle> BlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> input
) {
    try {
        constexpr uint32_t skipNBottomMips = 1;
        auto pyramidDesc = input->GetDesc()
            .HalfRes()
            .Format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            .AllMipLevels();
        
        uint32_t mipLevels = std::max(static_cast<int32_t>(pyramidDesc.GetMipLevels()) - static_cast<int32_t>(skipNBottomMips), 1);
        pyramidDesc.SetMipLevels(mipLevels);

        auto output = renderGraph.Create(pyramidDesc);

        tekki::render_graph::SimpleRenderPass::NewComputeRust(renderGraph.AddPass("_blur0"), "blur::blur_cs")
            .Read(input)
            .WriteView(output, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(0)
                .LevelCount(1))
            .Dispatch(output->GetDesc().GetExtent());

        for (uint32_t targetMip = 1; targetMip < output->GetDesc().GetMipLevels(); ++targetMip) {
            uint32_t downsampleAmount = 1u << targetMip;
            auto dispatchExtent = output->GetDesc().DivExtent({downsampleAmount, downsampleAmount, 1u}).GetExtent();

            tekki::render_graph::SimpleRenderPass::NewCompute(
                renderGraph.AddPass("_blur" + std::to_string(targetMip)),
                "/shaders/blur.hlsl"
            )
            .ReadView(output, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(targetMip - 1)
                .LevelCount(1))
            .WriteView(output, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(targetMip)
                .LevelCount(1))
            .Dispatch(dispatchExtent);
        }

        return output;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create blur pyramid: " + std::string(e.what()));
    }
}

std::shared_ptr<tekki::render_graph::ImageHandle> ReverseBlurPyramid(
    tekki::render_graph::RenderGraph& renderGraph,
    std::shared_ptr<tekki::render_graph::ImageHandle> inputPyramid
) {
    try {
        auto output = renderGraph.Create(*inputPyramid->GetDesc());

        for (uint32_t targetMip = output->GetDesc().GetMipLevels() - 1; targetMip > 0; --targetMip) {
            uint32_t downsampleAmount = 1u << targetMip;
            auto outputExtent = output->GetDesc().DivExtent({downsampleAmount, downsampleAmount, 1u}).GetExtent();
            uint32_t srcMip = targetMip + 1;
            float selfWeight = (srcMip == output->GetDesc().GetMipLevels()) ? 0.0f : 0.5f;

            tekki::render_graph::SimpleRenderPass::NewComputeRust(
                renderGraph.AddPass("_rev_blur" + std::to_string(targetMip)),
                "rev_blur::rev_blur_cs"
            )
            .ReadView(inputPyramid, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(targetMip)
                .LevelCount(1))
            .ReadView(output, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(srcMip)
                .LevelCount(1))
            .WriteView(output, tekki::backend::vulkan::ImageViewDesc::Builder()
                .BaseMipLevel(targetMip)
                .LevelCount(1))
            .Constants(std::make_tuple(outputExtent[0], outputExtent[1], selfWeight))
            .Dispatch(outputExtent);
        }

        return output;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create reverse blur pyramid: " + std::string(e.what()));
    }
}

} // namespace tekki::renderer::renderers