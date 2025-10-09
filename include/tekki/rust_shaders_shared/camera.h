#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tekki::rust_shaders_shared {

struct CameraMatrices {
    glm::mat4 ViewToClip;
    glm::mat4 ClipToView;
    glm::mat4 WorldToView;
    glm::mat4 ViewToWorld;

    CameraMatrices() = default;
    
    CameraMatrices(const CameraMatrices&) = default;
    CameraMatrices& operator=(const CameraMatrices&) = default;
    
    bool operator==(const CameraMatrices& other) const {
        return ViewToClip == other.ViewToClip &&
               ClipToView == other.ClipToView &&
               WorldToView == other.WorldToView &&
               ViewToWorld == other.ViewToWorld;
    }
    
    bool operator!=(const CameraMatrices& other) const {
        return !(*this == other);
    }

    /// Get the eye position in world space
    glm::vec3 EyePosition() const {
        glm::vec4 position = ViewToWorld * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return glm::vec3(position);
    }

    /// Get the eye direction in world space
    glm::vec3 EyeDirection() const {
        glm::vec4 direction = ViewToWorld * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        return glm::normalize(glm::vec3(direction));
    }

    /// Get the aspect ratio of the camera
    float AspectRatio() const {
        return ViewToClip[1][1] / ViewToClip[0][0];
    }
};

} // namespace tekki::rust_shaders_shared