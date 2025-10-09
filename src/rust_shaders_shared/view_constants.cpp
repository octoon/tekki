#include "tekki/rust_shaders_shared/view_constants.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace tekki::rust_shaders_shared {

ViewConstants VieportConstantBuilder::Build() const {
    try {
        glm::mat4 clip_to_prev_clip = prev_camera_matrices_.view_to_clip
            * prev_camera_matrices_.world_to_view
            * camera_matrices_.view_to_world
            * camera_matrices_.clip_to_view;

        ViewConstants res;
        res.ViewToClip = camera_matrices_.view_to_clip;
        res.ClipToView = camera_matrices_.clip_to_view;
        res.ViewToSample = glm::mat4(0.0f);
        res.SampleToView = glm::mat4(0.0f);
        res.WorldToView = camera_matrices_.world_to_view;
        res.ViewToWorld = camera_matrices_.view_to_world;

        res.ClipToPrevClip = clip_to_prev_clip;

        res.PrevViewToPrevClip = prev_camera_matrices_.view_to_clip;
        res.PrevClipToPrevView = prev_camera_matrices_.clip_to_view;
        res.PrevWorldToPrevView = prev_camera_matrices_.world_to_view;
        res.PrevViewToPrevWorld = prev_camera_matrices_.view_to_world;

        res.SampleOffsetPixels = glm::vec2(0.0f, 0.0f);
        res.SampleOffsetClip = glm::vec2(0.0f, 0.0f);

        res.SetPixelOffset(pixel_offset_, render_extent_);
        return res;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to build ViewConstants: " + std::string(e.what()));
    }
}

} // namespace tekki::rust_shaders_shared