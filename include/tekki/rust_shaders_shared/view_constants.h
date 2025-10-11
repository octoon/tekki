#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include "tekki/rust_shaders_shared/camera.h"

namespace tekki::rust_shaders_shared {

struct ViewConstants {
    glm::mat4 ViewToClip;
    glm::mat4 ClipToView;
    glm::mat4 ViewToSample;
    glm::mat4 SampleToView;
    glm::mat4 WorldToView;
    glm::mat4 ViewToWorld;

    glm::mat4 ClipToPrevClip;

    glm::mat4 PrevViewToPrevClip;
    glm::mat4 PrevClipToPrevView;
    glm::mat4 PrevWorldToPrevView;
    glm::mat4 PrevViewToPrevWorld;

    glm::vec2 SampleOffsetPixels;
    glm::vec2 SampleOffsetClip;

    ViewConstants() = default;
    ViewConstants(const ViewConstants&) = default;
    ViewConstants& operator=(const ViewConstants&) = default;

    /// `render_extent` is the Internal render resolution, before any upsampling:
    /// important for jittering over the whole domain.
    void SetPixelOffset(const glm::vec2& v, const glm::uvec2& render_extent) {
        glm::vec2 sample_offset_pixels = v;
        glm::vec2 sample_offset_clip = glm::vec2(
            (2.0f * v.x) / static_cast<float>(render_extent.x),
            (2.0f * v.y) / static_cast<float>(render_extent.y)
        );

        glm::mat4 jitter_matrix = glm::mat4(1.0f);
        jitter_matrix[3] = glm::vec4(-sample_offset_clip, 0.0f, 1.0f);

        glm::mat4 jitter_matrix_inv = glm::mat4(1.0f);
        jitter_matrix_inv[3] = glm::vec4(sample_offset_clip, 0.0f, 1.0f);

        glm::mat4 view_to_sample = jitter_matrix * ViewToClip;
        glm::mat4 sample_to_view = ClipToView * jitter_matrix_inv;

        ViewToSample = view_to_sample;
        SampleToView = sample_to_view;
        SampleOffsetPixels = sample_offset_pixels;
        SampleOffsetClip = sample_offset_clip;
    }

    glm::vec3 EyePosition() const {
        glm::vec4 eye_pos_h = ViewToWorld[3];
        return glm::vec3(eye_pos_h) / eye_pos_h.w;
    }

    glm::vec3 PrevEyePosition() const {
        glm::vec4 eye_pos_h = PrevViewToPrevWorld[3];
        return glm::vec3(eye_pos_h) / eye_pos_h.w;
    }
};

class VieportConstantBuilder {
private:
    glm::uvec2 render_extent_;
    CameraMatrices camera_matrices_;
    CameraMatrices prev_camera_matrices_;
    glm::vec2 pixel_offset_;

public:
    VieportConstantBuilder(const glm::uvec2& render_extent, 
                          const CameraMatrices& camera_matrices,
                          const CameraMatrices& prev_camera_matrices)
        : render_extent_(render_extent)
        , camera_matrices_(camera_matrices)
        , prev_camera_matrices_(prev_camera_matrices)
        , pixel_offset_(0.0f, 0.0f) {}

    template<typename CamMat>
    static VieportConstantBuilder Builder(const CamMat& camera_matrices, 
                                         const CamMat& prev_camera_matrices,
                                         const glm::uvec2& render_extent) {
        return VieportConstantBuilder(render_extent, 
                                     static_cast<CameraMatrices>(camera_matrices), 
                                     static_cast<CameraMatrices>(prev_camera_matrices));
    }

    VieportConstantBuilder& PixelOffset(const glm::vec2& v) {
        pixel_offset_ = v;
        return *this;
    }

    ViewConstants Build() const;
};

} // namespace tekki::rust_shaders_shared