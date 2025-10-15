#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/rust_shaders_shared/ssgi.h"
#include "tekki/renderer/renderers/ping_pong_temporal_resource.h"

namespace tekki::renderer::renderers {

struct GbufferDepth;

// The Rust shaders currently suffer a perfomance penalty. Tracking: https://github.com/EmbarkStudios/kajiya/issues/24
constexpr bool USE_RUST_SHADERS = false;

class SsgiRenderer {
public:
    SsgiRenderer();
    
    tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> Render(
        tekki::render_graph::TemporalRenderGraph& rg,
        const GbufferDepth& gbufferDepth,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& prevRadiance,
        VkDescriptorSet bindlessDescriptorSet
    );

private:
    PingPongTemporalResource ssgiTex;

    static tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image> FilterSsgi(
        tekki::render_graph::TemporalRenderGraph& rg,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& input,
        const GbufferDepth& gbufferDepth,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& reprojectionMap,
        PingPongTemporalResource& temporalTex
    );

    static tekki::render_graph::ImageDesc TemporalTexDesc(const glm::u32vec2& extent);

    static tekki::render_graph::Handle<tekki::backend::vulkan::Image> UpsampleSsgi(
        tekki::render_graph::RenderGraph& rg,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& ssgi,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& depth,
        const tekki::render_graph::Handle<tekki::backend::vulkan::Image>& gbuffer
    );

    static constexpr VkFormat INTERNAL_TEX_FMT = VK_FORMAT_R16_SFLOAT;
    static constexpr VkFormat FINAL_TEX_FMT = VK_FORMAT_R8_UNORM;
};

} // namespace tekki::renderer::renderers