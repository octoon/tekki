#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "frame_constants.h"
#include "util.h"

namespace tekki::rust_shaders_shared {

struct ViewRayContext {
    glm::vec4 RayDirCs;
    glm::vec4 RayDirVsH;
    glm::vec4 RayDirWsH;

    glm::vec4 RayOriginCs;
    glm::vec4 RayOriginVsH;
    glm::vec4 RayOriginWsH;

    glm::vec4 RayHitCs;
    glm::vec4 RayHitVsH;
    glm::vec4 RayHitWsH;

    glm::vec3 RayDirVs() const {
        return glm::vec3(RayDirVsH.x, RayDirVsH.y, RayDirVsH.z);
    }

    glm::vec3 RayDirWs() const {
        return glm::vec3(RayDirWsH.x, RayDirWsH.y, RayDirWsH.z);
    }

    glm::vec3 RayOriginVs() const {
        return glm::vec3(RayOriginVsH.x / RayOriginVsH.w, RayOriginVsH.y / RayOriginVsH.w, RayOriginVsH.z / RayOriginVsH.w);
    }

    glm::vec3 RayOriginWs() const {
        return glm::vec3(RayOriginWsH.x / RayOriginWsH.w, RayOriginWsH.y / RayOriginWsH.w, RayOriginWsH.z / RayOriginWsH.w);
    }

    glm::vec3 RayHitVs() const {
        return glm::vec3(RayHitVsH.x / RayHitVsH.w, RayHitVsH.y / RayHitVsH.w, RayHitVsH.z / RayHitVsH.w);
    }

    glm::vec3 RayHitWs() const {
        return glm::vec3(RayHitWsH.x / RayHitWsH.w, RayHitWsH.y / RayHitWsH.w, RayHitWsH.z / RayHitWsH.w);
    }

    static ViewRayContext FromUv(const glm::vec2& uv, const FrameConstants& frameConstants) {
        const auto& viewConstants = frameConstants.view_constants;

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
    }

    static ViewRayContext FromUvAndDepth(const glm::vec2& uv, float depth, const FrameConstants& frameConstants) {
        const auto& viewConstants = frameConstants.view_constants;

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
    }
};

} // namespace tekki::rust_shaders_shared