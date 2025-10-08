#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

class DeferredRenderer {
public:
    DeferredRenderer();

    GbufferDepth render(
        render_graph::RenderGraph& rg,
        const std::array<uint32_t, 2>& output_extent,
        vk::DescriptorSet bindless_descriptor_set
    );

private:
    // Deferred rendering state
    std::shared_ptr<vulkan::Buffer> deferred_constants_buffer_;
};

} // namespace tekki::renderer