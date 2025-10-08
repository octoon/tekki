#pragma once

#include "tekki/render_graph/graph.h"
#include "tekki/backend/vk_sync/AccessType.h"
#include "tekki/backend/vulkan/image/Image.h"

namespace tekki {
namespace render_graph {

void clearDepth(RenderGraph& rg, Handle<backend::Image>& img);
void clearColor(RenderGraph& rg, Handle<backend::Image>& img, const std::array<float, 4>& clearColor);

} // namespace render_graph
} // namespace tekki