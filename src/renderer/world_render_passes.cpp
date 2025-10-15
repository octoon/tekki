#include "tekki/renderer/world_render_passes.h"
#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <stdexcept>
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/lib.h"
#include "tekki/renderer/world_frame_desc.h"
#include "renderers/deferred/light_gbuffer.h"
#include "renderers/motion_blur/motion_blur.h"
#include "renderers/raster_meshes/raster_meshes.h"
#include "renderers/reference/reference_path_trace.h"
#include "renderers/shadows/trace_sun_shadow_mask.h"
#include "renderers/sky/sky.h"
#include "renderers/reprojection/reprojection.h"
#include "renderers/wrc/wrc.h"
#include "renderers/ircache/ircache.h"
#include "renderers/rtdgi/rtdgi.h"
#include "renderers/rtr/rtr.h"
#include "renderers/lighting/lighting.h"
#include "renderers/taa/taa.h"
#include "renderers/post/post.h"
#include "world_renderer.h"

namespace tekki::renderer {

std::shared_ptr<Image> WorldRenderer::PrepareRenderGraphStandard(
    std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph,
    const WorldFrameDesc& frameDesc
) {
    try {
        auto tlas = renderGraph->device()->ray_tracing_enabled() 
            ? std::make_optional(PrepareTopLevelAcceleration(renderGraph))
            : std::nullopt;

        auto accum_img = renderGraph->GetOrCreateTemporal(
            "root.accum",
            ImageDesc::New2D(VK_FORMAT_R16G16B16A16_SFLOAT, frameDesc.render_extent).Usage(
                VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT
            )
        );

        auto sky_cube = Ibl->Render(renderGraph).value_or(
            sky::RenderSkyCube(renderGraph)
        );

        auto convolved_sky_cube = sky::ConvolveCube(renderGraph, sky_cube);

        GbufferDepth gbuffer_depth;
        std::shared_ptr<Image> velocity_img;

        {
            auto normal = renderGraph->Create(ImageDesc::New2D(
                VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                frameDesc.render_extent
            ));

            auto gbuffer = renderGraph->Create(ImageDesc::New2D(
                VK_FORMAT_R32G32B32A32_SFLOAT,
                frameDesc.render_extent
            ));

            auto depth_img = renderGraph->Create(ImageDesc::New2D(
                VK_FORMAT_D32_SFLOAT,
                frameDesc.render_extent
            ));
            render_graph::imageops::ClearDepth(renderGraph, depth_img);

            gbuffer_depth = GbufferDepth(normal, gbuffer, depth_img);

            velocity_img = renderGraph->Create(ImageDesc::New2D(
                VK_FORMAT_R16G16B16A16_SFLOAT,
                frameDesc.render_extent
            ));

            RasterMeshes(
                renderGraph,
                RasterSimpleRenderPass,
                gbuffer_depth,
                velocity_img,
                RasterMeshesData{
                    .meshes = Meshes,
                    .instances = Instances,
                    .vertex_buffer = VertexBuffer->lock(),
                    .bindless_descriptor_set = BindlessDescriptorSet
                }
            );
        }

        auto reprojection_map = reprojection::CalculateReprojectionMap(
            renderGraph,
            gbuffer_depth,
            velocity_img
        );

        auto ssgi_tex = Ssgi->Render(
            renderGraph,
            gbuffer_depth,
            reprojection_map,
            accum_img,
            BindlessDescriptorSet
        );

        auto ircache_state = Ircache->Prepare(renderGraph);

        auto wrc = wrc::AllocateDummyOutput(renderGraph);

        std::optional<std::shared_ptr<Image>> traced_ircache;
        if (tlas.has_value()) {
            traced_ircache = ircache_state->TraceIrradiance(
                renderGraph,
                convolved_sky_cube,
                BindlessDescriptorSet,
                tlas.value(),
                wrc
            );
        }

        auto sun_shadow_mask = tlas.has_value()
            ? TraceSunShadowMask(renderGraph, gbuffer_depth, tlas.value(), BindlessDescriptorSet)
            : renderGraph->Create(gbuffer_depth.depth->desc().Format(VK_FORMAT_R8_UNORM));

        auto reprojected_rtdgi = Rtdgi->Reproject(renderGraph, reprojection_map);

        auto denoised_shadow_mask = (SunSizeMultiplier > 0.0f)
            ? ShadowDenoise->Render(renderGraph, gbuffer_depth, sun_shadow_mask, reprojection_map)
            : sun_shadow_mask;

        if (traced_ircache.has_value()) {
            ircache_state->SumUpIrradianceForSampling(renderGraph, traced_ircache.value());
        }

        std::optional<std::shared_ptr<Image>> rtdgi_irradiance;
        std::optional<std::shared_ptr<Image>> rtdgi_candidates;

        if (tlas.has_value()) {
            auto rtdgi_result = Rtdgi->Render(
                renderGraph,
                reprojected_rtdgi,
                gbuffer_depth,
                reprojection_map,
                convolved_sky_cube,
                BindlessDescriptorSet,
                ircache_state,
                wrc,
                tlas.value(),
                ssgi_tex
            );
            rtdgi_irradiance = rtdgi_result.screen_irradiance_tex;
            rtdgi_candidates = rtdgi_result.candidates;
        }

        bool any_triangle_lights = false;
        for (const auto& inst : Instances) {
            if (!MeshLights[inst.mesh.index].lights.empty()) {
                any_triangle_lights = true;
                break;
            }
        }

        auto rtr_result = (tlas.has_value() && rtdgi_irradiance.has_value() && rtdgi_candidates.has_value())
            ? Rtr->Trace(
                renderGraph,
                gbuffer_depth,
                reprojection_map,
                sky_cube,
                BindlessDescriptorSet,
                tlas.value(),
                rtdgi_irradiance.value(),
                rtdgi_candidates.value(),
                ircache_state,
                wrc
            )
            : Rtr->CreateDummyOutput(renderGraph, gbuffer_depth);

        if (any_triangle_lights && tlas.has_value()) {
            Lighting->RenderSpecular(
                rtr_result.resolved_tex,
                renderGraph,
                gbuffer_depth,
                BindlessDescriptorSet,
                tlas.value()
            );
        }

        auto rtr = rtr_result.FilterTemporal(renderGraph, gbuffer_depth, reprojection_map);

        auto debug_out_tex = renderGraph->Create(ImageDesc::New2D(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            gbuffer_depth.gbuffer->desc().extent_2d()
        ));

        auto final_rtdgi = rtdgi_irradiance.has_value()
            ? rtdgi_irradiance.value()
            : renderGraph->Create(ImageDesc::New2D(VK_FORMAT_R8G8B8A8_UNORM, glm::u32vec2(1, 1)));

        LightGbuffer(
            renderGraph,
            gbuffer_depth,
            denoised_shadow_mask,
            rtr,
            final_rtdgi,
            ircache_state,
            wrc,
            accum_img,
            debug_out_tex,
            sky_cube,
            convolved_sky_cube,
            BindlessDescriptorSet,
            DebugShadingMode,
            DebugShowWrc
        );

        std::optional<std::shared_ptr<Image>> anti_aliased;

#ifdef WITH_DLSS
        if (UseDlss) {
            anti_aliased = Dlss->Render(
                renderGraph,
                debug_out_tex,
                reprojection_map,
                gbuffer_depth.depth,
                TemporalUpscaleExtent
            );
        }
#endif

        if (!anti_aliased.has_value()) {
            anti_aliased = Taa->Render(
                renderGraph,
                debug_out_tex,
                reprojection_map,
                gbuffer_depth.depth,
                TemporalUpscaleExtent
            ).this_frame_out;
        }

        auto final_post_input = MotionBlur(
            renderGraph,
            anti_aliased.value(),
            gbuffer_depth.depth,
            reprojection_map
        );

        if (tlas.has_value() && DebugMode == RenderDebugMode::WorldRadianceCache) {
            wrc->SeeThrough(
                renderGraph,
                convolved_sky_cube,
                ircache_state,
                BindlessDescriptorSet,
                tlas.value(),
                final_post_input
            );
        }

        auto post_processed = Post->Render(
            renderGraph,
            final_post_input,
            BindlessDescriptorSet,
            ExposureState().post_mult,
            Contrast,
            DynamicExposure.histogram_clipping
        );

        auto debugged_resource = renderGraph->TakeDebuggedResource();
        return debugged_resource.has_value() ? debugged_resource.value() : post_processed;

    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("PrepareRenderGraphStandard failed: ") + e.what());
    }
}

std::shared_ptr<Image> WorldRenderer::PrepareRenderGraphReference(
    std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph,
    const WorldFrameDesc& frameDesc
) {
    try {
        auto accum_img = renderGraph->GetOrCreateTemporal(
            "refpt.accum",
            ImageDesc::New2D(VK_FORMAT_R32G32B32A32_SFLOAT, frameDesc.render_extent).Usage(
                VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT
            )
        );

        if (ResetReferenceAccumulation) {
            ResetReferenceAccumulation = false;
            render_graph::imageops::ClearColor(renderGraph, accum_img, {0.0f, 0.0f, 0.0f, 0.0f});
        }

        if (renderGraph->device()->ray_tracing_enabled()) {
            auto tlas = PrepareTopLevelAcceleration(renderGraph);
            ReferencePathTrace(renderGraph, accum_img, BindlessDescriptorSet, tlas);
        }

        return Post->Render(
            renderGraph,
            accum_img,
            BindlessDescriptorSet,
            ExposureState().post_mult,
            Contrast,
            DynamicExposure.histogram_clipping
        );

    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("PrepareRenderGraphReference failed: ") + e.what());
    }
}

std::shared_ptr<Image> WorldRenderer::PrepareTopLevelAcceleration(std::shared_ptr<render_graph::TemporalRenderGraph> renderGraph) {
    // Implementation depends on specific acceleration structure creation
    // This is a placeholder - actual implementation would create and return the TLAS
    throw std::runtime_error("PrepareTopLevelAcceleration not implemented");
}

} // namespace tekki::renderer