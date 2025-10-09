#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <memory>
#include <stdexcept>

namespace tekki::rust_shaders {

struct Constants {
    uint32_t output_extent_x;
    uint32_t output_extent_y;
    float self_weight;
};

inline glm::vec4 Lerp(const glm::vec4& from, const glm::vec4& to, float t) {
    return from * (1.0f - t) + to * t;
}

class RevBlur {
public:
    static void ComputeShader(
        const class Image2D& input_tail_tex,
        const class Image2D& input_tex,
        class Image2D& output_tex,
        const class Sampler& sampler_lnc,
        const Constants& constants,
        const glm::uvec3& px
    ) {
        try {
            glm::vec4 pyramid_col = input_tail_tex.Fetch(glm::uvec2(px.x, px.y));

            glm::vec4 self_col;
            glm::vec2 inv_size = glm::vec2(1.0f) / glm::vec2(
                static_cast<float>(constants.output_extent_x),
                static_cast<float>(constants.output_extent_y)
            );
            
            if (true) {
                // TODO: do a small Gaussian blur instead of this nonsense

                const int32_t K = 1;
                self_col = glm::vec4(0.0f);

                for (int32_t y = -K; y <= K; ++y) {
                    for (int32_t x = -K; x <= K; ++x) {
                        glm::vec2 uv = (glm::vec2(px.x, px.y) + glm::vec2(0.5f) + glm::vec2(static_cast<float>(x), static_cast<float>(y))) * inv_size;
                        glm::vec4 sample = input_tex.SampleByLod(sampler_lnc, uv, 0.0f);
                        self_col += sample;
                    }
                }

                self_col /= static_cast<float>((2 * K + 1) * (2 * K + 1));
            } else {
                glm::vec2 uv = (glm::vec2(px.x, px.y) + glm::vec2(0.5f)) * inv_size;
                //float4 self_col = input_tex[px / 2];
                self_col = input_tex.SampleByLod(sampler_lnc, uv, 0.0f);
            }

            float exponential_falloff = 0.6f;

            // BUG: when `self_weight` is 1.0, the `w` here should be 1.0, not `exponential_falloff`
            output_tex.Write(
                glm::uvec2(px.x, px.y),
                Lerp(
                    self_col,
                    pyramid_col,
                    constants.self_weight * exponential_falloff
                )
            );
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("RevBlur compute shader failed: ") + e.what());
        }
    }
};

} // namespace tekki::rust_shaders