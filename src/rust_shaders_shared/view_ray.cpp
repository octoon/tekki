#include "tekki/rust_shaders_shared/view_ray.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "frame_constants.h"
#include "util.h"
#include <stdexcept>

namespace tekki::rust_shaders_shared {

glm::vec3 ViewRayContext::RayDirVs() const {
    return glm::vec3(RayDirVsH.x, RayDirVsH.y, RayDirVsH.z);
}

glm::vec3 ViewRayContext::RayDirWs() const {
    return glm::vec3(RayDirWsH.x, RayDirWsH.y, RayDirWsH.z);
}

glm::vec3 ViewRayContext::RayOriginVs() const {
    return glm::vec3(RayOriginVsH.x / RayOriginVsH.w, RayOriginVsH.y / RayOriginVsH.w, RayOriginVsH.z / RayOriginVsH.w);
}

glm::vec3 ViewRayContext::RayOriginWs() const {
    return glm::vec3(RayOriginWsH.x / RayOriginWsH.w, RayOriginWsH.y / RayOriginWsH.w, RayOriginWsH.z / RayOriginWsH.w);
}

glm::vec3 ViewRayContext::RayHitVs() const {
    return glm::vec3(RayHitVsH.x / RayHitVsH.w, RayHitVsH.y / RayHitVsH.w, RayHitVsH.z / RayHitVsH.w);
}

glm::vec3 ViewRayContext::RayHitWs() const {
    return glm::vec3(RayHitWsH.x / RayHitWsH.w, RayHitWsH.y / RayHitWsH.w, RayHitWsH.z / RayHitWsH.w);
}

ViewRayContext ViewRayContext::FromUv(const glm::vec2& uv, const FrameConstants& frameConstants) {
    try {
        const auto& viewConstants = frameConstants.ViewConstants;

        glm::vec4 rayDirCs = glm::vec4(UvToCs(uv), 0.0f, 1.0f);
        glm::vec4 rayDirVsH = viewConstants.SampleToView * rayDirCs;
        glm::vec4 rayDirWsH = viewConstants.ViewToWorld * rayDirVsH;

        glm::vec4 rayOriginCs = glm::vec4(UvToCs(uv), 1.0f, 1.0f);
        glm::vec4 rayOriginVsH = viewConstants.SampleToView * rayOriginCs;
        glm::vec4 rayOriginWsH = viewConstants.ViewToWorld * rayOriginVsH;

        return ViewRayContext{
            rayDirCs,
            rayDirVsH,
            rayDirWsH,
            rayOriginCs,
            rayOriginVsH,
            rayOriginWsH,
            glm::vec4(0.0f),
            glm::vec4(0.0f),
            glm::vec4(0.0f)
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create ViewRayContext from UV: " + std::string(e.what()));
    }
}

ViewRayContext ViewRayContext::FromUvAndDepth(const glm::vec2& uv, float depth, const FrameConstants& frameConstants) {
    try {
        const auto& viewConstants = frameConstants.ViewConstants;

        glm::vec4 rayDirCs = glm::vec4(UvToCs(uv), 0.0f, 1.0f);
        glm::vec4 rayDirVsH = viewConstants.SampleToView * rayDirCs;
        glm::vec4 rayDirWsH = viewConstants.ViewToWorld * rayDirVsH;

        glm::vec4 rayOriginCs = glm::vec4(UvToCs(uv), 1.0f, 1.0f);
        glm::vec4 rayOriginVsH = viewConstants.SampleToView * rayOriginCs;
        glm::vec4 rayOriginWsH = viewConstants.ViewToWorld * rayOriginVsH;

        glm::vec4 rayHitCs = glm::vec4(UvToCs(uv), depth, 1.0f);
        glm::vec4 rayHitVsH = viewConstants.SampleToView * rayHitCs;
        glm::vec4 rayHitWsH = viewConstants.ViewToWorld * rayHitVsH;

        return ViewRayContext{
            rayDirCs,
            rayDirVsH,
            rayDirWsH,
            rayOriginCs,
            rayOriginVsH,
            rayOriginWsH,
            rayHitCs,
            rayHitVsH,
            rayHitWsH
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create ViewRayContext from UV and depth: " + std::string(e.what()));
    }
}

} // namespace tekki::rust_shaders_shared