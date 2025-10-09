#include "tekki/rust_shaders_shared/camera.h"

namespace tekki::rust_shaders_shared {

CameraMatrices::CameraMatrices() = default;

CameraMatrices::CameraMatrices(const CameraMatrices&) = default;

CameraMatrices& CameraMatrices::operator=(const CameraMatrices&) = default;

bool CameraMatrices::operator==(const CameraMatrices& other) const {
    return ViewToClip == other.ViewToClip &&
           ClipToView == other.ClipToView &&
           WorldToView == other.WorldToView &&
           ViewToWorld == other.ViewToWorld;
}

bool CameraMatrices::operator!=(const CameraMatrices& other) const {
    return !(*this == other);
}

/// Get the eye position in world space
glm::vec3 CameraMatrices::EyePosition() const {
    glm::vec4 position = ViewToWorld * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return glm::vec3(position);
}

/// Get the eye direction in world space
glm::vec3 CameraMatrices::EyeDirection() const {
    glm::vec4 direction = ViewToWorld * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(direction));
}

/// Get the aspect ratio of the camera
float CameraMatrices::AspectRatio() const {
    return ViewToClip[1][1] / ViewToClip[0][0];
}

} // namespace tekki::rust_shaders_shared