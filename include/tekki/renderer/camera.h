#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

namespace tekki::renderer {

struct CameraMatrices {
    glm::mat4 ViewToClip;
    glm::mat4 ClipToView;
    glm::mat4 WorldToView;
    glm::mat4 ViewToWorld;
};

class CameraBodyMatrices {
public:
    glm::mat4 WorldToView;
    glm::mat4 ViewToWorld;

    CameraBodyMatrices(glm::mat4 worldToView, glm::mat4 viewToWorld)
        : WorldToView(worldToView), ViewToWorld(viewToWorld) {}

    static CameraBodyMatrices FromPositionRotation(glm::vec3 position, glm::quat rotation) {
        glm::mat4 viewToWorld = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
        glm::mat4 worldToView = glm::mat4_cast(glm::conjugate(rotation)) * glm::translate(glm::mat4(1.0f), -position);
        
        return CameraBodyMatrices(worldToView, viewToWorld);
    }

    bool operator==(const CameraBodyMatrices& other) const {
        return WorldToView == other.WorldToView && ViewToWorld == other.ViewToWorld;
    }

    bool operator!=(const CameraBodyMatrices& other) const {
        return !(*this == other);
    }
};

class CameraLensMatrices {
public:
    glm::mat4 ViewToClip;
    glm::mat4 ClipToView;

    CameraLensMatrices(glm::mat4 viewToClip, glm::mat4 clipToView)
        : ViewToClip(viewToClip), ClipToView(clipToView) {}
};

class CameraLens {
public:
    float NearPlaneDistance;
    float AspectRatio;
    float VerticalFov;

    CameraLens() : NearPlaneDistance(0.01f), AspectRatio(1.0f), VerticalFov(52.0f) {}

    CameraLens(float nearPlaneDistance, float aspectRatio, float verticalFov)
        : NearPlaneDistance(nearPlaneDistance), AspectRatio(aspectRatio), VerticalFov(verticalFov) {}

    CameraLensMatrices CalcMatrices() const {
        float fov = glm::radians(VerticalFov);
        float znear = NearPlaneDistance;

        float h = glm::cos(0.5f * fov) / glm::sin(0.5f * fov);
        float w = h / AspectRatio;

        glm::mat4 viewToClip = glm::mat4(
            glm::vec4(w, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, h, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, -1.0f),
            glm::vec4(0.0f, 0.0f, znear, 0.0f)
        );

        glm::mat4 clipToView = glm::mat4(
            glm::vec4(1.0f / w, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 1.0f / h, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f / znear),
            glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
        );

        return CameraLensMatrices(viewToClip, clipToView);
    }
};

class IntoCameraBodyMatrices {
public:
    virtual CameraBodyMatrices GetCameraBodyMatrices() = 0;
    virtual ~IntoCameraBodyMatrices() = default;
};

class LookThroughCamera {
public:
    virtual CameraMatrices Through(const CameraLens& lens) = 0;
    virtual ~LookThroughCamera() = default;
};

template<typename T>
class LookThroughCameraWrapper : public LookThroughCamera {
private:
    T value;

public:
    LookThroughCameraWrapper(T&& val) : value(std::forward<T>(val)) {}

    CameraMatrices Through(const CameraLens& lens) override {
        auto body = value.GetCameraBodyMatrices();
        auto lensMatrices = lens.CalcMatrices();

        return CameraMatrices{
            lensMatrices.ViewToClip,
            lensMatrices.ClipToView,
            body.WorldToView,
            body.ViewToWorld
        };
    }
};

template<>
class LookThroughCameraWrapper<CameraBodyMatrices> : public LookThroughCamera {
private:
    CameraBodyMatrices value;

public:
    LookThroughCameraWrapper(CameraBodyMatrices val) : value(val) {}

    CameraMatrices Through(const CameraLens& lens) override {
        auto lensMatrices = lens.CalcMatrices();
        
        return CameraMatrices{
            lensMatrices.ViewToClip,
            lensMatrices.ClipToView,
            value.WorldToView,
            value.ViewToWorld
        };
    }
};

template<>
class LookThroughCameraWrapper<std::pair<glm::vec3, glm::quat>> : public LookThroughCamera {
private:
    std::pair<glm::vec3, glm::quat> value;

public:
    LookThroughCameraWrapper(std::pair<glm::vec3, glm::quat> val) : value(val) {}

    CameraMatrices Through(const CameraLens& lens) override {
        auto body = CameraBodyMatrices::FromPositionRotation(value.first, value.second);
        auto lensMatrices = lens.CalcMatrices();
        
        return CameraMatrices{
            lensMatrices.ViewToClip,
            lensMatrices.ClipToView,
            body.WorldToView,
            body.ViewToWorld
        };
    }
};

} // namespace tekki::renderer