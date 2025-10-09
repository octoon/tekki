#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

namespace tekki::renderer {

// Motion Blur effect renderer
// Implements per-pixel motion blur using velocity vectors
class MotionBlurRenderer {
public:
    // Apply motion blur effect to input image
    // Parameters:
    //   rg: Render graph for resource management
    //   input: Input color image
    //   depth: Depth buffer for motion calculations
    //   reprojection_map: Velocity/reprojection vectors
    //   motion_blur_scale: Scale factor for blur intensity (default 1.0)
    // Returns: Motion blurred image
    static render_graph::Handle<vulkan::Image> render_motion_blur(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        float motion_blur_scale = 1.0f
    );

private:
    static constexpr uint32_t VELOCITY_TILE_SIZE = 16;

    // Reduce velocity in X direction using tiles
    static render_graph::Handle<vulkan::Image> velocity_reduce_x(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& reprojection_map
    );

    // Reduce velocity in Y direction using tiles
    static render_graph::Handle<vulkan::Image> velocity_reduce_y(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& velocity_reduced_x
    );

    // Dilate velocity to expand high-motion areas
    static render_graph::Handle<vulkan::Image> velocity_dilate(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& velocity_reduced_y
    );

    // Apply motion blur using dilated velocity
    static render_graph::Handle<vulkan::Image> apply_motion_blur(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& input,
        const render_graph::Handle<vulkan::Image>& depth,
        const render_graph::Handle<vulkan::Image>& reprojection_map,
        const render_graph::Handle<vulkan::Image>& velocity_dilated,
        float motion_blur_scale
    );
};

} // namespace tekki::renderer