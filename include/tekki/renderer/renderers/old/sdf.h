#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/RenderGraph.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/render_graph/RenderPass.h"

namespace tekki::renderer::renderers::old {

class SdfRasterBricks {
public:
    std::shared_ptr<Buffer> BrickMetaBuffer;
    std::shared_ptr<Buffer> BrickInstBuffer;
};

class RasterSdfData {
public:
    std::shared_ptr<Image> SdfImg;
    std::shared_ptr<Buffer> BrickInstBuffer;
    std::shared_ptr<Buffer> BrickMetaBuffer;
    std::shared_ptr<Buffer> CubeIndexBuffer;
};

class SdfRenderer {
public:
    static std::shared_ptr<Image> RaymarchSdf(
        std::shared_ptr<RenderGraph> renderGraph,
        std::shared_ptr<Image> sdfImage,
        ImageDesc description
    );

    static void EditSdf(
        std::shared_ptr<RenderGraph> renderGraph,
        std::shared_ptr<Image> sdfImage,
        bool clear
    );

    static std::shared_ptr<Buffer> ClearSdfBricksMeta(
        std::shared_ptr<RenderGraph> renderGraph
    );

    static SdfRasterBricks CalculateSdfBricksMeta(
        std::shared_ptr<RenderGraph> renderGraph,
        std::shared_ptr<Image> sdfImage
    );

    static void RasterSdf(
        std::shared_ptr<RenderGraph> renderGraph,
        std::shared_ptr<RenderPass> renderPass,
        std::shared_ptr<Image> depthImage,
        std::shared_ptr<Image> colorImage,
        RasterSdfData rasterSdfData
    );

    static std::vector<uint32_t> CubeIndices();
};

} // namespace tekki::renderer::renderers::old