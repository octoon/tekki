#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/Result.h"

namespace tekki::rust_shaders {

class Sky {
public:
    static glm::vec3 AtmosphereDefault(const glm::vec3& wi, const glm::vec3& lightDir, const glm::vec3& lightColor) {
        //return Vec3::ZERO;
        //return Vec3::splat(0.5);

        glm::vec3 worldSpaceCameraPos = glm::vec3(0.0f);
        glm::vec3 rayStart = worldSpaceCameraPos;
        glm::vec3 rayDir = glm::normalize(wi);
        float rayLength = std::numeric_limits<float>::infinity();

        glm::vec3 transmittance = glm::vec3(0.0f);
        return IntegrateScattering(
            rayStart,
            rayDir,
            rayLength,
            lightDir,
            lightColor,
            transmittance
        );
    }

    static void CompSkyCubeCs(
        const std::vector<float>& outputTex,
        const FrameConstants& frameConstants,
        const glm::uvec3& px
    ) {
        try {
            uint32_t face = px.z;
            glm::vec2 uv = glm::vec2(px.x + 0.5f, px.y + 0.5f) / 64.0f;
            glm::vec3 dir = CubeMapFaceRotations[face] * glm::vec3(uv * 2.0f - glm::vec2(1.0f), -1.0f);

            glm::vec3 output = AtmosphereDefault(
                dir,
                glm::vec3(frameConstants.SunDirection),
                glm::vec3(frameConstants.SunColorMultiplier) * frameConstants.PreExposure
            );
            
            // output_tex.write(px, output.extend(1.0));
            // Note: Texture writing implementation depends on specific graphics API
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Error in CompSkyCubeCs: " + std::string(e.what()));
        }
    }

private:
    static glm::vec3 IntegrateScattering(
        const glm::vec3& rayStart,
        const glm::vec3& rayDir,
        float rayLength,
        const glm::vec3& lightDir,
        const glm::vec3& lightColor,
        glm::vec3& transmittance
    ) {
        // Implementation of atmospheric scattering integration
        // This would need to be implemented based on the actual scattering model
        return glm::vec3(0.0f);
    }

    static constexpr std::array<glm::mat3, 6> CubeMapFaceRotations = {
        // Define cube map face rotations here
        glm::mat3(1.0f), glm::mat3(1.0f), glm::mat3(1.0f),
        glm::mat3(1.0f), glm::mat3(1.0f), glm::mat3(1.0f)
    };
};

} // namespace tekki::rust_shaders