#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/Result.h"
#include "tekki/asset/Image.h"
#include "tekki/mesh/TexParams.h"
#include "tekki/mesh/GpuImage.h"
#include "tekki/renderer/WorldRenderer.h"
#include "tekki/renderer/LutRenderers.h"
#include "tekki/backend/RenderBackend.h"
#include "tekki/cache/LazyCache.h"

namespace tekki::renderer {

class DefaultWorldRenderer {
public:
    static WorldRenderer Create(
        const glm::uvec2& renderExtent,
        const glm::uvec2& temporalUpscaleExtent,
        const std::shared_ptr<RenderBackend>& backend,
        const std::shared_ptr<LazyCache>& lazyCache
    ) {
        WorldRenderer worldRenderer = WorldRenderer::CreateEmpty(renderExtent, temporalUpscaleExtent, backend);

        // BINDLESS_LUT_BRDF_FG
        worldRenderer.AddImageLut(BrdfFgLutComputer(), 0);

        {
            auto image = tekki::asset::Image::LoadFromPath("/images/bluenoise/256_256/LDR_RGBA_0.png");
            auto blueNoiseImage = std::make_shared<GpuImage>();
            
            TexParams params;
            params.Gamma = TexGamma::Linear;
            params.UseMips = false;
            params.Compression = TexCompressionMode::None;
            params.ChannelSwizzle = std::nullopt;

            auto uploadTask = std::make_shared<UploadGpuImage>(image, params, backend->GetDevice());
            auto lazyResult = uploadTask->Evaluate(lazyCache);
            
            if (!lazyResult->IsReady()) {
                throw std::runtime_error("Failed to upload blue noise image");
            }
            
            auto blueNoiseImg = lazyResult->GetResult();
            auto handle = worldRenderer.AddImage(blueNoiseImg);

            // BINDLESS_LUT_BLUE_NOISE_256_LDR_RGBA_0
            if (handle.GetIndex() != 1) {
                throw std::runtime_error("Unexpected blue noise image handle index");
            }
        }

        // BINDLESS_LUT_BEZOLD_BRUCKE
        worldRenderer.AddImageLut(BezoldBruckeLutComputer(), 2);

        // Build an empty TLAS to create the resources. We'll update it at runtime.
        if (backend->GetDevice()->IsRayTracingEnabled()) {
            worldRenderer.BuildRayTracingTopLevelAcceleration();
        }

        return worldRenderer;
    }
};

}