#pragma once

#include <memory>
#include <optional>
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/resource.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

// Forward declarations
namespace half_res {
    rg::Handle<Image> ExtractHalfResGbufferViewNormalRGBA8(rg::RenderGraph& rg, const rg::Handle<Image>& gbuffer);
    rg::Handle<Image> ExtractHalfResDepth(rg::RenderGraph& rg, const rg::Handle<Image>& depth);
}

/**
 * Gbuffer and depth information for rendering
 * Ported from kajiya's GbufferDepth struct
 */
struct GbufferDepth {
    rg::Handle<Image> geometric_normal;
    rg::Handle<Image> gbuffer;
    rg::Handle<Image> depth;

    // Cached half-resolution versions
    mutable std::optional<rg::Handle<Image>> half_view_normal_cache;
    mutable std::optional<rg::Handle<Image>> half_depth_cache;

    GbufferDepth(
        rg::Handle<Image> geometricNormal,
        rg::Handle<Image> gbuffer_,
        rg::Handle<Image> depth_
    ) : geometric_normal(std::move(geometricNormal)),
        gbuffer(std::move(gbuffer_)),
        depth(std::move(depth_)),
        half_view_normal_cache(std::nullopt),
        half_depth_cache(std::nullopt) {}

    // Get or create half-resolution view normal
    const rg::Handle<Image>& half_view_normal(rg::RenderGraph& rg) const {
        if (!half_view_normal_cache) {
            half_view_normal_cache = half_res::ExtractHalfResGbufferViewNormalRGBA8(rg, gbuffer);
        }
        return *half_view_normal_cache;
    }

    // Get or create half-resolution depth
    const rg::Handle<Image>& half_depth(rg::RenderGraph& rg) const {
        if (!half_depth_cache) {
            half_depth_cache = half_res::ExtractHalfResDepth(rg, depth);
        }
        return *half_depth_cache;
    }
};

} // namespace tekki::renderer::renderers
