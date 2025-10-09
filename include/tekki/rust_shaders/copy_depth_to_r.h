#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"

namespace tekki::rust_shaders {

class CopyDepthToR {
public:
    /**
     * Copy depth to R compute shader
     * @param inputTex Input texture (2D, f32, sampled=true)
     * @param outputTex Output texture (2D, f32, sampled=false)
     * @param globalInvocationId Global invocation ID
     */
    static void CopyDepthToRCs(
        const std::shared_ptr<void>& inputTex,
        const std::shared_ptr<void>& outputTex,
        const glm::uvec3& globalInvocationId
    );
};

} // namespace tekki::rust_shaders