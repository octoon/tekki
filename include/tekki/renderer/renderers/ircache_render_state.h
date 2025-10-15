#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/render_graph/lib.h"

namespace tekki::renderer::renderers {

// IRCache render state
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

} // namespace tekki::renderer::renderers
