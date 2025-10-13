#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/lib.h"
#include "tekki/renderer/renderers/prefix_scan.h"
#include "tekki/renderer/renderers/wrc_render_state.h"
#include "tekki/rust_shaders_shared/frame_constants.h"

namespace tekki::renderer::renderers {

constexpr size_t MAX_GRID_CELLS = 32 * 32 * 32 * 8;
constexpr size_t MAX_ENTRIES = 1024 * 64;
constexpr float IRCACHE_GRID_CELL_DIAMETER = 0.16f * 0.125f;
constexpr size_t IRCACHE_CASCADE_SIZE = 32;
constexpr size_t IRCACHE_SAMPLES_PER_FRAME = 4;
constexpr size_t IRCACHE_VALIDATION_SAMPLES_PER_FRAME = 4;

struct IrcacheRenderState {
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheMetaBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheGridMetaBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheGridMetaBuf2;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheEntryCellBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheSpatialBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheIrradianceBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheAuxBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheLifeBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcachePoolBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheEntryIndirectionBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheRepositionProposalBuf;
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IrcacheRepositionProposalCountBuf;
    bool PendingIrradianceSum;
};

struct IrcacheIrradiancePendingSummation {
    tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> IndirectArgsBuf;
};

class IrcacheRenderer {
public:
    IrcacheRenderer(std::shared_ptr<tekki::backend::vulkan::Device> device);

    void UpdateEyePosition(const glm::vec3& eyePosition);
    std::array<rust_shaders_shared::IrcacheCascadeConstants, 8> Constants() const;
    glm::vec3 GridCenter() const;
    IrcacheRenderState Prepare(tekki::render_graph::TemporalRenderGraph& rg);

private:
    std::shared_ptr<tekki::backend::vulkan::RenderPass> DebugRenderPass;
    bool Initialized;
    glm::vec3 GridCenter_;
    std::array<glm::ivec3, 8> CurScroll;
    std::array<glm::ivec3, 8> PrevScroll;
    size_t Parity;
    bool EnableScroll;
};

} // namespace tekki::renderer::renderers