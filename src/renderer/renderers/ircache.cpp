#include "tekki/renderer/renderers/ircache.h"
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

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;

// Helper function to convert backend BufferDesc to render graph BufferDesc
rg::BufferDesc ConvertToRenderGraphDesc(const tekki::backend::vulkan::BufferDesc& backendDesc) {
    rg::BufferDesc rgDesc;
    rgDesc.size = backendDesc.size;
    rgDesc.usage = backendDesc.usage;
    rgDesc.memory_location = backendDesc.memory_location;
    rgDesc.alignment = backendDesc.alignment;
    return rgDesc;
}

IrcacheRenderer::IrcacheRenderer(std::shared_ptr<tekki::backend::vulkan::Device> device)
    : DebugRenderPass(nullptr)
    , Initialized(false)
    , GridCenter_(glm::vec3(0.0f))
    , CurScroll()
    , PrevScroll()
    , Parity(0)
    , EnableScroll(true)
{
    (void)device;  // Will be used when render pass creation is properly implemented
    // TODO: Implement render pass creation when needed
    // For now, just initialize to nullptr
    DebugRenderPass = nullptr;
}

void IrcacheRenderer::UpdateEyePosition(const glm::vec3& eyePosition)
{
    if (!EnableScroll) {
        return;
    }

    GridCenter_ = eyePosition;

    for (size_t cascade = 0; cascade < 8; ++cascade) {
        float cellDiameter = IRCACHE_GRID_CELL_DIAMETER * static_cast<float>(1 << cascade);
        glm::ivec3 cascadeCenter = glm::ivec3(glm::floor(eyePosition / cellDiameter));
        glm::ivec3 cascadeOrigin = cascadeCenter - glm::ivec3(IRCACHE_CASCADE_SIZE / 2);

        PrevScroll[cascade] = CurScroll[cascade];
        CurScroll[cascade] = cascadeOrigin;
    }
}

std::array<rust_shaders_shared::IrcacheCascadeConstants, 8> IrcacheRenderer::Constants() const
{
    std::array<rust_shaders_shared::IrcacheCascadeConstants, 8> constants;

    for (size_t cascade = 0; cascade < 8; ++cascade) {
        glm::ivec3 curScroll = CurScroll[cascade];
        glm::ivec3 prevScroll = PrevScroll[cascade];
        glm::ivec3 scrollAmount = curScroll - prevScroll;

        rust_shaders_shared::IrcacheCascadeConstants& c = constants[cascade];
        c.origin = glm::ivec4(curScroll.x, curScroll.y, curScroll.z, 0);
        c.voxels_scrolled_this_frame = glm::ivec4(scrollAmount.x, scrollAmount.y, scrollAmount.z, 0);
    }

    return constants;
}

glm::vec3 IrcacheRenderer::GridCenter() const
{
    return GridCenter_;
}

IrcacheRenderState IrcacheRenderer::Prepare(tekki::render_graph::TemporalRenderGraph& rg)
{
    constexpr size_t INDIRECTION_BUF_ELEM_COUNT = 1024 * 1024;
    static_assert(INDIRECTION_BUF_ELEM_COUNT >= MAX_ENTRIES, "INDIRECTION_BUF_ELEM_COUNT must be >= MAX_ENTRIES");

    auto temporal_storage_buffer = [&](const std::string& name, size_t size) -> tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> {
        auto backendDesc = tekki::backend::vulkan::BufferDesc::NewGpuOnly(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto rgDesc = ConvertToRenderGraphDesc(backendDesc);
        return rg.GetOrCreateTemporal(rg::TemporalResourceKey(name), rgDesc);
    };

    IrcacheRenderState state{
        temporal_storage_buffer("ircache.meta_buf", sizeof(uint32_t) * 8),
        temporal_storage_buffer("ircache.grid_meta_buf", sizeof(uint32_t[2]) * MAX_GRID_CELLS),
        temporal_storage_buffer("ircache.grid_meta_buf2", sizeof(uint32_t[2]) * MAX_GRID_CELLS),
        temporal_storage_buffer("ircache.entry_cell_buf", sizeof(uint32_t) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.spatial_buf", sizeof(glm::vec4) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.irradiance_buf", 3 * sizeof(glm::vec4) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.aux_buf", 4 * 16 * sizeof(glm::vec4) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.life_buf", sizeof(uint32_t) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.pool_buf", sizeof(uint32_t) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.entry_indirection_buf", sizeof(uint32_t) * INDIRECTION_BUF_ELEM_COUNT),
        temporal_storage_buffer("ircache.reposition_proposal_buf", sizeof(glm::vec4) * MAX_ENTRIES),
        temporal_storage_buffer("ircache.reposition_proposal_count_buf", sizeof(uint32_t) * MAX_ENTRIES),
        false
    };

    if (Parity == 1) {
        std::swap(state.IrcacheGridMetaBuf, state.IrcacheGridMetaBuf2);
    }

    if (!Initialized) {
        auto pass = rg.AddPass("clear ircache pool");
        auto simplePass = tekki::render_graph::SimpleRenderPass::NewCompute(
            pass, "/shaders/ircache/clear_ircache_pool.hlsl");
        
        simplePass
            .Write(state.IrcachePoolBuf)
            .Write(state.IrcacheLifeBuf)
            .Dispatch(glm::uvec3(MAX_ENTRIES, 1, 1));

        Initialized = true;
    } else {
        auto pass = rg.AddPass("scroll cascades");
        auto simplePass = tekki::render_graph::SimpleRenderPass::NewCompute(
            pass, "/shaders/ircache/scroll_cascades.hlsl");
        
        simplePass
            .Read(state.IrcacheGridMetaBuf)
            .Write(state.IrcacheGridMetaBuf2)
            .Write(state.IrcacheEntryCellBuf)
            .Write(state.IrcacheIrradianceBuf)
            .Write(state.IrcacheLifeBuf)
            .Write(state.IrcachePoolBuf)
            .Write(state.IrcacheMetaBuf)
            .Dispatch(glm::uvec3(IRCACHE_CASCADE_SIZE, IRCACHE_CASCADE_SIZE, IRCACHE_CASCADE_SIZE * 8));

        std::swap(state.IrcacheGridMetaBuf, state.IrcacheGridMetaBuf2);
        Parity = (Parity + 1) % 2;
    }

    auto indirect_args_buf = [&]() -> tekki::render_graph::Handle<tekki::backend::vulkan::Buffer> {
        tekki::backend::vulkan::BufferDesc backendDesc;
        backendDesc.size = (sizeof(uint32_t) * 4) * 2;
        backendDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        backendDesc.memory_location = tekki::MemoryLocation::GpuOnly;

        auto rgDesc = ConvertToRenderGraphDesc(backendDesc);
        auto buf = rg.Create(rgDesc);

        auto pass = rg.AddPass("_ircache dispatch args");
        auto simplePass = tekki::render_graph::SimpleRenderPass::NewCompute(
            pass, "/shaders/ircache/prepare_age_dispatch_args.hlsl");

        simplePass
            .Write(state.IrcacheMetaBuf)
            .Write(buf)
            .Dispatch(glm::uvec3(1, 1, 1));

        return buf;
    }();

    tekki::backend::vulkan::BufferDesc entryOccupancyDesc;
    entryOccupancyDesc.size = sizeof(uint32_t) * MAX_ENTRIES;
    entryOccupancyDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    entryOccupancyDesc.memory_location = tekki::MemoryLocation::GpuOnly;

    auto rgEntryOccupancyDesc = ConvertToRenderGraphDesc(entryOccupancyDesc);
    auto entry_occupancy_buf = rg.Create(rgEntryOccupancyDesc);

    auto agePass = rg.AddPass("age ircache entries");
    auto ageSimplePass = tekki::render_graph::SimpleRenderPass::NewCompute(
        agePass, "/shaders/ircache/age_ircache_entries.hlsl");
    
    ageSimplePass
        .Write(state.IrcacheMetaBuf)
        .Write(state.IrcacheGridMetaBuf)
        .Write(state.IrcacheEntryCellBuf)
        .Write(state.IrcacheLifeBuf)
        .Write(state.IrcachePoolBuf)
        .Write(state.IrcacheSpatialBuf)
        .Write(state.IrcacheRepositionProposalBuf)
        .Write(state.IrcacheRepositionProposalCountBuf)
        .Write(state.IrcacheIrradianceBuf)
        .Write(entry_occupancy_buf)
        .Dispatch(glm::uvec3(MAX_ENTRIES, 1, 1));

    auto renderGraphPtr = std::shared_ptr<tekki::render_graph::RenderGraph>(rg.GetRenderGraph(), [](tekki::render_graph::RenderGraph*) {
        // No delete - the TemporalRenderGraph owns the RenderGraph
    });
    PrefixScan::InclusivePrefixScanU32_1M(renderGraphPtr, entry_occupancy_buf);

    auto compactPass = rg.AddPass("ircache compact");
    auto compactSimplePass = tekki::render_graph::SimpleRenderPass::NewCompute(
        compactPass, "/shaders/ircache/ircache_compact_entries.hlsl");
    
    compactSimplePass
        .Write(state.IrcacheMetaBuf)
        .Write(state.IrcacheLifeBuf)
        .Read(entry_occupancy_buf)
        .Write(state.IrcacheEntryIndirectionBuf)
        .Dispatch(glm::uvec3(MAX_ENTRIES, 1, 1));

    return state;
}

} // namespace tekki::renderer::renderers