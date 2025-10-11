#pragma once

#include <vector>
#include <memory>
#include <array>
#include <stdexcept>
#include <string>
#include <glm/glm.hpp>
#include "tekki/rust_shaders_shared/frame_constants.h"

namespace tekki::rust_shaders {

class ExtractHalfResDepth {
public:
    /**
     * Extract half resolution depth
     */
    static void Execute(
        const glm::uvec2& dispatch_id,
        [[maybe_unused]] const std::shared_ptr<void>& input_texture,
        [[maybe_unused]] const std::shared_ptr<void>& output_texture,
        const tekki::rust_shaders_shared::FrameConstants& frameConstants
    ) {
        try {
            const glm::uvec2 px = dispatch_id;
            
            const std::array<glm::ivec2, 4> hi_px_subpixels = {
                glm::ivec2(0, 0),
                glm::ivec2(1, 1),
                glm::ivec2(1, 0),
                glm::ivec2(0, 1)
            };
            
            const glm::ivec2 src_px = glm::ivec2(px) * 2 + hi_px_subpixels[(frameConstants.frame_index & 3)];
            
            // Note: Texture fetching and writing operations would be implemented
            // using the specific graphics API (Vulkan, DirectX, etc.)
            // The actual implementation would depend on the rendering backend
            
            // input_tex.fetch(src_px) equivalent would be implemented here
            // output_tex.write(px, input) equivalent would be implemented here
            
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("ExtractHalfResDepth execution failed: ") + e.what());
        }
    }
};

} // namespace tekki::rust_shaders