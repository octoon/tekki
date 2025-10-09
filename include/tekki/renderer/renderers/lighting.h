#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/lib.h"

namespace tekki::renderer::renderers {

class LightingRenderer {
public:
    LightingRenderer();
    ~LightingRenderer() = default;

    LightingRenderer(const LightingRenderer&) = delete;
    LightingRenderer& operator=(const LightingRenderer&) = delete;
    LightingRenderer(LightingRenderer&&) = delete;
    LightingRenderer& operator=(LightingRenderer&&) = delete;

    void RenderSpecular(
        rg::Handle<tekki::backend::vulkan::Image>& outputTex,
        rg::TemporalRenderGraph& rg,
        const GbufferDepth& gbufferDepth,
        vk::DescriptorSet bindlessDescriptorSet,
        const rg::Handle<tekki::backend::vulkan::RayTracingAcceleration>& tlas
    );

private:
    struct SpatialResolveOffsets {
        static constexpr std::array<glm::ivec2, 8> OFFSETS = {
            glm::ivec2(0, 0),
            glm::ivec2(1, 0),
            glm::ivec2(0, 1),
            glm::ivec2(1, 1),
            glm::ivec2(2, 0),
            glm::ivec2(0, 2),
            glm::ivec2(2, 1),
            glm::ivec2(1, 2)
        };
    };
};

} // namespace tekki::renderer::renderers