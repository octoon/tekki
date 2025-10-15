#include "tekki/renderer/renderers/old/csgi.h"
#include <array>
#include <memory>
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

CascadeScroll::CascadeScroll() : Scroll{0, 0, 0, 0} {}

std::array<int32_t, 4> CascadeScroll::VolumeScrollOffsetFrom(const CascadeScroll& other) const {
    std::array<int32_t, 4> result;
    for (size_t i = 0; i < 4; ++i) {
        result[i] = (Scroll[i] - other.Scroll[i]);
        result[i] = std::clamp(result[i], -static_cast<int32_t>(VOLUME_DIMS), static_cast<int32_t>(VOLUME_DIMS));
    }
    return result;
}

void CsgiVolume::DebugRasterVoxelGrid(
    tekki::render_graph::RenderGraph* renderGraph,
    std::shared_ptr<tekki::backend::vulkan::RenderPass> renderPass,
    class GbufferDepth* gbufferDepth,
    tekki::render_graph::Handle<tekki::backend::vulkan::Image>* velocityImage,
    size_t cascadeIndex
) {
    // Implementation depends on specific render graph and GbufferDepth implementation
    // This is a placeholder that matches the Rust function signature
}

void CsgiVolume::FullscreenDebugRadiance(
    tekki::render_graph::RenderGraph* renderGraph,
    tekki::render_graph::Handle<tekki::backend::vulkan::Image>* output
) {
    // Implementation depends on specific render graph implementation
    // This is a placeholder that matches the Rust function signature
}

CsgiRenderer::CsgiRenderer() : 
    traceSubdiv_(3),
    neighborsPerFrame_(2),
    frameIndex_(0),
    currentScroll_(),
    previousScroll_() {
}

glm::vec3 CsgiRenderer::GridCenter(const glm::vec3& focusPosition, float cascadeScale) {
    float voxelSize = VoxelSize(cascadeScale);
    return glm::round(focusPosition / voxelSize) * voxelSize;
}

float CsgiRenderer::VolumeSize(float giScale) {
    return 12.0f * VOLUME_WORLD_SCALE_MULT * giScale;
}

float CsgiRenderer::VoxelSize(float giScale) {
    return VolumeSize(giScale) / static_cast<float>(VOLUME_DIMS);
}

void CsgiRenderer::UpdateEyePosition(const glm::vec3& eyePosition, float giScale) {
    // The grid is shifted, so the _center_ and not _origin_ of cascade 0 is at origin.
    // This moves the seams in the origin-centered grid away from origin.
    // Must match GPU code. Search token: 3e7ddeec-afbb-44e4-8b75-b54276c6df2b
    float gridOffsetWorldSpace = -VoxelSize(giScale) * (static_cast<float>(VOLUME_DIMS) / 2.0f);

    // Transform the eye position to the grid-local space
    glm::vec3 localEyePosition = eyePosition - glm::vec3(gridOffsetWorldSpace);

    std::array<float, CASCADE_COUNT> cascadeScales;
    for (size_t i = 0; i < CASCADE_COUNT; ++i) {
        cascadeScales[i] = std::pow(CASCADE_EXP_SCALING_RATIO, static_cast<int>(i));
    }

    std::array<CascadeScroll, CASCADE_COUNT> giVolumeScroll;
    for (size_t cascade_i = 0; cascade_i < CASCADE_COUNT; ++cascade_i) {
        float cascadeScale = giScale * cascadeScales[cascade_i];
        float voxelSize = VoxelSize(cascadeScale);

        glm::vec3 giGridCenter = GridCenter(localEyePosition, cascadeScale);
        glm::vec3 giVolumeScrollVec = giGridCenter / voxelSize;

        CascadeScroll scroll;
        if (SCROLL_CASCADES) {
            glm::vec3 giVolumeScrollAdjusted = giVolumeScrollVec + glm::sign(giVolumeScrollVec) * 0.5f;
            scroll.Scroll = {
                static_cast<int32_t>(giVolumeScrollAdjusted.x) - static_cast<int32_t>(VOLUME_DIMS) / 2,
                static_cast<int32_t>(giVolumeScrollAdjusted.y) - static_cast<int32_t>(VOLUME_DIMS) / 2,
                static_cast<int32_t>(giVolumeScrollAdjusted.z) - static_cast<int32_t>(VOLUME_DIMS) / 2,
                0
            };
        } else {
            scroll.Scroll = {0, 0, 0, 0};
        }
        giVolumeScroll[cascade_i] = scroll;
    }

    previousScroll_ = currentScroll_;
    currentScroll_ = giVolumeScroll;
}

std::array<GiCascadeConstants, CASCADE_COUNT> CsgiRenderer::Constants(float giScale) const {
    std::array<float, CASCADE_COUNT> cascadeScales;
    for (size_t i = 0; i < CASCADE_COUNT; ++i) {
        cascadeScales[i] = std::pow(CASCADE_EXP_SCALING_RATIO, static_cast<int>(i));
    }

    std::array<GiCascadeConstants, CASCADE_COUNT> result;
    for (size_t cascade_i = 0; cascade_i < CASCADE_COUNT; ++cascade_i) {
        float cascadeScale = giScale * cascadeScales[cascade_i];
        float volumeSize = VolumeSize(cascadeScale);
        float voxelSize = VoxelSize(cascadeScale);

        std::array<int32_t, 4> giVolumeScrollFrac = {0, 0, 0, 0};
        std::array<int32_t, 4> giVolumeScrollInt = {0, 0, 0, 0};

        auto giVolumeScroll = currentScroll_[cascade_i].Scroll;
        for (size_t k = 0; k < 4; ++k) {
            int32_t i = giVolumeScroll[k];
            int32_t g = (i / static_cast<int32_t>(VOLUME_DIMS)) * static_cast<int32_t>(VOLUME_DIMS);
            int32_t r = i - g;
            giVolumeScrollFrac[k] = r;
            giVolumeScrollInt[k] = g;
        }

        glm::ivec4 voxelsScrolledThisFrame;
        auto scrollOffset = currentScroll_[cascade_i].VolumeScrollOffsetFrom(previousScroll_[cascade_i]);
        for (size_t i = 0; i < 4; ++i) {
            voxelsScrolledThisFrame[i] = scrollOffset[i];
        }

        GiCascadeConstants constants;
        for (size_t i = 0; i < 4; ++i) {
            constants.scroll_frac[i] = giVolumeScrollFrac[i];
            constants.scroll_int[i] = giVolumeScrollInt[i];
        }
        constants.voxels_scrolled_this_frame = voxelsScrolledThisFrame;
        constants.volume_size = volumeSize;
        constants.voxel_size = voxelSize;
        // Set other default values as needed

        result[cascade_i] = constants;
    }

    return result;
}

CsgiVolume CsgiRenderer::Render(
    const glm::vec3& eyePosition,
    tekki::render_graph::TemporalRenderGraph* renderGraph,
    const tekki::render_graph::Handle<tekki::backend::vulkan::Image>* skyCube,
    VkDescriptorSet bindlessDescriptorSet,
    const tekki::render_graph::Handle<tekki::backend::vulkan::RayTracingAcceleration>* tlas
) {
    CsgiVolume volume = CreateVolume(renderGraph);
    auto& directCascades = volume.Direct;
    auto& indirectCombinedCascades = volume.Indirect;
    auto& indirectCascades = volume.SubrayIndirect;
    auto& opacityCascades = volume.Opacity;

    // Stagger cascade updates over frames
    size_t cascadeUpdateMask = ~0ULL;

    // Advance quantum values as necessary (frame index slowed down to account for the stager).
    uint32_t quantumIdx = frameIndex_ / CASCADE_COUNT;

    uint32_t sweepVxCount = VOLUME_DIMS >> std::clamp(traceSubdiv_, 0, 5);

    // Note: The sweep shaders should be dispatched as individual slices, and synchronized
    // in-between with barries (no cache flushes), so that propagation can happen without race conditions.
    // At the very least, it could be dispatched as one slice, but looping inside the shaders would use atomic
    // work stealing instead of slice iteration.
    //
    // Either is going to leave the GPU quite under-utilized though, and would be a nice fit for async compute.

    for (size_t cascade_i = 0; cascade_i < CASCADE_COUNT; ++cascade_i) {
        if (0 == ((1 << cascade_i) & cascadeUpdateMask)) {
            continue;
        }

        // SimpleRenderPass::new_compute for decay_volume
        // Implementation depends on specific render graph implementation

        // SimpleRenderPass::new_rt for trace_volume
        // Implementation depends on specific render graph implementation
    }

    auto nullCascade = renderGraph->Create(
        tekki::render_graph::ImageDesc::New3d(
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            glm::u32vec3(1, 1, 1)
        ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT)
    );

    for (int cascade_i = CASCADE_COUNT - 1; cascade_i >= 0; --cascade_i) {
        if (0 == ((1 << cascade_i) & cascadeUpdateMask)) {
            continue;
        }

        auto& directOpacity = opacityCascades[cascade_i];

        // SimpleRenderPass::new_compute for direct_opacity_sum
        // Implementation depends on specific render graph implementation

        auto outerCascadeSubrayIndirect = (cascade_i + 1 < CASCADE_COUNT) ? &indirectCascades[cascade_i + 1] : &nullCascade;
        auto& indirectCascade = indirectCascades[cascade_i];

        // SimpleRenderPass::new_compute for diagonal_sweep_volume
        // Implementation depends on specific render graph implementation

        // SimpleRenderPass::new_compute for sweep_volume
        // Implementation depends on specific render graph implementation
    }

    frameIndex_++;

    return volume;
}

CsgiVolume CsgiRenderer::CreateVolume(tekki::render_graph::TemporalRenderGraph* renderGraph) const {
    return CreateVolumeWithDimensions(
        renderGraph,
        {VOLUME_DIMS * CARDINAL_DIRECTION_COUNT, VOLUME_DIMS, VOLUME_DIMS},
        {VOLUME_DIMS * TOTAL_SUBRAY_COUNT, VOLUME_DIMS, VOLUME_DIMS},
        {VOLUME_DIMS * TOTAL_DIRECTION_COUNT, VOLUME_DIMS, VOLUME_DIMS},
        {VOLUME_DIMS, VOLUME_DIMS, VOLUME_DIMS}
    );
}

CsgiVolume CsgiRenderer::CreateDummyVolume(tekki::render_graph::TemporalRenderGraph* renderGraph) const {
    return CreateVolumeWithDimensions(renderGraph, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
}

CsgiVolume CsgiRenderer::CreateVolumeWithDimensions(
    tekki::render_graph::TemporalRenderGraph* renderGraph,
    const std::array<uint32_t, 3>& directCascadeDimensions,
    const std::array<uint32_t, 3>& indirectCascadeDimensions,
    const std::array<uint32_t, 3>& indirectCombinedCascadeDimensions,
    const std::array<uint32_t, 3>& opacityCascadeDimensions
) const {
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> directCascades;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> indirectCascades;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> indirectCombinedCascades;
    std::array<tekki::render_graph::Handle<tekki::backend::vulkan::Image>, CASCADE_COUNT> opacityCascades;

    for (size_t i = 0; i < CASCADE_COUNT; ++i) {
        directCascades[i] = renderGraph->GetOrCreateTemporal(
            "csgi.direct_cascade" + std::to_string(i),
            tekki::render_graph::ImageDesc::New3d(
                VK_FORMAT_R16G16B16A16_SFLOAT,
                glm::u32vec3(directCascadeDimensions[0], directCascadeDimensions[1], directCascadeDimensions[2])
            ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        indirectCascades[i] = renderGraph->GetOrCreateTemporal(
            "csgi.indirect_cascade" + std::to_string(i),
            tekki::render_graph::ImageDesc::New3d(
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                glm::u32vec3(indirectCascadeDimensions[0], indirectCascadeDimensions[1], indirectCascadeDimensions[2])
            ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        indirectCombinedCascades[i] = renderGraph->GetOrCreateTemporal(
            "csgi.indirect_cascade_combined" + std::to_string(i),
            tekki::render_graph::ImageDesc::New3d(
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                glm::u32vec3(indirectCombinedCascadeDimensions[0], indirectCombinedCascadeDimensions[1], indirectCombinedCascadeDimensions[2])
            ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );

        opacityCascades[i] = renderGraph->Create(
            tekki::render_graph::ImageDesc::New3d(
                VK_FORMAT_R8_UNORM,
                glm::u32vec3(opacityCascadeDimensions[0], opacityCascadeDimensions[1], opacityCascadeDimensions[2])
            ).WithUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
        );
    }

    return CsgiVolume{
        directCascades,
        indirectCombinedCascades,
        indirectCascades,
        opacityCascades
    };
}

} // namespace tekki::renderer::renderers::old