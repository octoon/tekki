#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/lib.h"
#include "tekki/rust_shaders_shared/frame_constants.h"

namespace tekki::renderer::renderers::old {

// Cone sweep global illumination prototype

// VOLUME_DIMS and CASCADE_COUNT must match GPU code.
// Seach token: d4109bba-438f-425e-8667-19e591be9a56
constexpr uint32_t VOLUME_DIMS = 64;
constexpr size_t CASCADE_COUNT = 1;
constexpr bool SCROLL_CASCADES = false;
constexpr float VOLUME_WORLD_SCALE_MULT = 1.0f;

// Search token: b518ed19-c715-4cc7-9bc7-e0dbbca3e037
constexpr float CASCADE_EXP_SCALING_RATIO = 2.0f;

struct CascadeScroll {
    std::array<int32_t, 4> Scroll;

    CascadeScroll() : Scroll{0, 0, 0, 0} {}
    
    std::array<int32_t, 4> VolumeScrollOffsetFrom(const CascadeScroll& other) const {
        std::array<int32_t, 4> result;
        for (size_t i = 0; i < 4; ++i) {
            result[i] = (Scroll[i] - other.Scroll[i]);
            result[i] = std::clamp(result[i], -static_cast<int32_t>(VOLUME_DIMS), static_cast<int32_t>(VOLUME_DIMS));
        }
        return result;
    }
};

struct CsgiVolume {
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> Direct;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> Indirect;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> SubrayIndirect;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> Opacity;

    void DebugRasterVoxelGrid(
        tekki::render_graph::RenderGraph* renderGraph,
        std::shared_ptr<tekki::backend::vulkan::RenderPass> renderPass,
        class GbufferDepth* gbufferDepth,
        tekki::render_graph::Handle<tekki::backend::vulkan::Image>* velocityImage,
        size_t cascadeIndex
    );

    void FullscreenDebugRadiance(
        tekki::render_graph::RenderGraph* renderGraph,
        tekki::render_graph::Handle<tekki::backend::vulkan::Image>* output
    );
};

class CsgiRenderer {
private:
    int32_t traceSubdiv_;
    int32_t neighborsPerFrame_;
    uint32_t frameIndex_;
    std::array<CascadeScroll, CASCADE_COUNT> currentScroll_;
    std::array<CascadeScroll, CASCADE_COUNT> previousScroll_;

    static glm::vec3 GridCenter(const glm::vec3& focusPosition, float cascadeScale);

public:
    CsgiRenderer();
    
    static float VolumeSize(float giScale);
    static float VoxelSize(float giScale);
    
    void UpdateEyePosition(const glm::vec3& eyePosition, float giScale);
    std::array<GiCascadeConstants, CASCADE_COUNT> Constants(float giScale) const;
    
    CsgiVolume Render(
        const glm::vec3& eyePosition,
        tekki::render_graph::TemporalRenderGraph* renderGraph,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>* skyCube,
        VkDescriptorSet bindlessDescriptorSet,
        const tekki::render_graph::Handle<tekki::backend::vulkan::RayTracingAcceleration>* tlas
    );
    
    CsgiVolume CreateVolume(tekki::render_graph::TemporalRenderGraph* renderGraph) const;
    CsgiVolume CreateDummyVolume(tekki::render_graph::TemporalRenderGraph* renderGraph) const;
    
private:
    CsgiVolume CreateVolumeWithDimensions(
        tekki::render_graph::TemporalRenderGraph* renderGraph,
        const std::array<uint32_t, 3>& directCascadeDimensions,
        const std::array<uint32_t, 3>& indirectCascadeDimensions,
        const std::array<uint32_t, 3>& indirectCombinedCascadeDimensions,
        const std::array<uint32_t, 3>& opacityCascadeDimensions
    ) const;
};

constexpr size_t CARDINAL_DIRECTION_COUNT = 6;
constexpr size_t CARDINAL_SUBRAY_COUNT = 5;
constexpr size_t DIAGONAL_DIRECTION_COUNT = 8;
constexpr size_t DIAGONAL_SUBRAY_COUNT = 3;
constexpr size_t TOTAL_DIRECTION_COUNT = CARDINAL_DIRECTION_COUNT + DIAGONAL_DIRECTION_COUNT;
constexpr size_t TOTAL_SUBRAY_COUNT = CARDINAL_DIRECTION_COUNT * CARDINAL_SUBRAY_COUNT + DIAGONAL_DIRECTION_COUNT * DIAGONAL_SUBRAY_COUNT;

} // namespace tekki::renderer::renderers::old