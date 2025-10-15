#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/asset/image.h"
#include "tekki/asset/TexParams.h"
#include "tekki/asset/mesh.h"
#include "tekki/renderer/world_renderer.h"
#include "tekki/renderer/lut_renderers.h"
#include "tekki/renderer/image_cache.h"
#include "tekki/backend/render_backend.h"

namespace tekki::renderer {

class DefaultWorldRenderer {
public:
    static WorldRenderer Create(
        const glm::uvec2& renderExtent,
        const glm::uvec2& temporalUpscaleExtent,
        const std::shared_ptr<RenderBackend>& backend
    ) {
        WorldRenderer worldRenderer = WorldRenderer::CreateEmpty(renderExtent, temporalUpscaleExtent, backend);

        // BINDLESS_LUT_BRDF_FG
        worldRenderer.AddImageLut(std::make_unique<BrdfFgLutComputer>(), 0);

        {
            auto image = tekki::asset::RawImage::LoadFromPath("/images/bluenoise/256_256/LDR_RGBA_0.png");

            tekki::asset::TexParams params;
            params.gamma = tekki::asset::TexGamma::Linear;
            params.use_mips = false;
            params.Compression = tekki::asset::TexCompressionMode::None;
            params.ChannelSwizzle = std::nullopt;

            UploadGpuImage uploadTask(image, params, backend->Device);
            auto blueNoiseImg = uploadTask.Execute();

            auto handle = worldRenderer.AddImage(blueNoiseImg);

            // BINDLESS_LUT_BLUE_NOISE_256_LDR_RGBA_0
            if (handle.GetIndex() != 1) {
                throw std::runtime_error("Unexpected blue noise image handle index");
            }
        }

        // BINDLESS_LUT_BEZOLD_BRUCKE
        worldRenderer.AddImageLut(std::make_unique<BezoldBruckeLutComputer>(), 2);

        // Build an empty TLAS to create the resources. We'll update it at runtime.
        if (backend->Device->IsRayTracingEnabled()) {
            worldRenderer.BuildRayTracingTopLevelAcceleration();
        }

        return worldRenderer;
    }
};

}