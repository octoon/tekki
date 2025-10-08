#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/buffer.h"
#include "../../../include/tekki/shared/camera.h"

#include <memory>
#include <optional>
#include <glm/glm.hpp>

namespace tekki::renderer {

// Import CameraMatrices from shared namespace
using shared::CameraMatrices;

// Render overrides for debug and testing
struct RenderOverrides {
    bool disable_ssgi = false;
    bool disable_rtr = false;
    bool disable_rtdgi = false;
    bool disable_shadows = false;
    bool disable_taa = false;
    bool disable_post_processing = false;
};

// Mesh instance (forward declaration here, defined in WorldRenderer.h)
struct MeshInstance;
struct MeshHandle;

// Forward declarations for renderers
class PostProcessRenderer;
class SsgiRenderer;
class RtrRenderer;
class LightingRenderer;
class IrcacheRenderer;
class RtdgiRenderer;
class TaaRenderer;
class ShadowDenoiseRenderer;
class IblRenderer;

// G-buffer and depth structure
struct GbufferDepth {
    render_graph::Handle<vulkan::Image> geometric_normal;
    render_graph::Handle<vulkan::Image> gbuffer;
    render_graph::Handle<vulkan::Image> depth;

    std::optional<render_graph::Handle<vulkan::Image>> half_view_normal;
    std::optional<render_graph::Handle<vulkan::Image>> half_depth;

    GbufferDepth(
        render_graph::Handle<vulkan::Image> geometric_normal,
        render_graph::Handle<vulkan::Image> gbuffer,
        render_graph::Handle<vulkan::Image> depth
    );

    render_graph::Handle<vulkan::Image> get_half_view_normal(render_graph::RenderGraph& rg);
    render_graph::Handle<vulkan::Image> get_half_depth(render_graph::RenderGraph& rg);
};

// Ping-pong temporal resource
struct PingPongTemporalResource {
    render_graph::TemporalResourceKey output_tex;
    render_graph::TemporalResourceKey history_tex;

    PingPongTemporalResource(const std::string& name);

    std::pair<
        render_graph::Handle<vulkan::Image>,
        render_graph::Handle<vulkan::Image>
    > get_output_and_history(
        render_graph::TemporalRenderGraph& rg,
        const vulkan::ImageDesc& desc
    );
};

// Image LUT
struct ImageLut {
    std::shared_ptr<vulkan::Image> image;
    bool computed = false;

    ImageLut(std::shared_ptr<vulkan::Image> img) : image(std::move(img)) {}

    void compute_if_needed(render_graph::RenderGraph& rg) {
        if (computed) {
            return;
        }

        // TODO: Implement LUT computation logic
        // This would involve:
        // 1. Importing the image into the render graph
        // 2. Running compute shader to generate LUT data
        // 3. Exporting the image for shader access

        computed = true;
    }

    std::shared_ptr<vulkan::Image> backing_image() const {
        return image;
    }
};

} // namespace tekki::renderer