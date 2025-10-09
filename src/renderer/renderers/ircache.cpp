#include "../../include/tekki/renderer/renderers/ircache.h"

#include "../../backend/vulkan/shader.h"
#include "../../include/tekki/renderer/renderers/renderers.h"
#include "../../include/tekki/shared/frame_constants.h"

#include <glm/glm.hpp>

namespace tekki::renderer
{

// IRCache constants based on original Rust implementation
constexpr float IRCACHE_GRID_CELL_DIAMETER = 0.16f * 0.125f;
constexpr uint32_t IRCACHE_CASCADE_SIZE = 32;
constexpr uint32_t IRCACHE_SAMPLES_PER_FRAME = 4;
constexpr uint32_t IRCACHE_VALIDATION_SAMPLES_PER_FRAME = 4;
constexpr uint32_t MAX_ENTRIES = 1024 * 64;
constexpr uint32_t MAX_GRID_CELLS = IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE * shared::IRCACHE_CASCADE_COUNT;

IrcacheRenderer::IrcacheRenderer()
    : ircache_tex_("ircache"), eye_position_(0.0f), grid_center_(0.0f), initialized_(false), parity_(0), enable_scroll_(true)
{
    // Initialize IRCache cascades with default values
    ircache_cascades_.resize(shared::IRCACHE_CASCADE_COUNT);
    for (auto& cascade : ircache_cascades_) {
        cascade.origin = glm::ivec4(0);
        cascade.voxels_scrolled_this_frame = glm::ivec4(0);
    }

    // Initialize scroll tracking
    cur_scroll_.resize(shared::IRCACHE_CASCADE_COUNT, glm::ivec3(0));
    prev_scroll_.resize(shared::IRCACHE_CASCADE_COUNT, glm::ivec3(0));
}

render_graph::ReadOnlyHandle<vulkan::Image>
IrcacheRenderer::render(render_graph::TemporalRenderGraph& rg, const GbufferDepth& gbuffer_depth,
                     const render_graph::Handle<vulkan::Image>& reprojection_map,
                     const render_graph::Handle<vulkan::Image>& prev_radiance,
                     vk::DescriptorSet bindless_descriptor_set)
{
    auto [ircache_output, ircache_history] = ircache_tex_.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(rg.resource_desc(gbuffer_depth.geometric_normal).extent[0],
                                      rg.resource_desc(gbuffer_depth.geometric_normal).extent[1],
                                      vk::Format::eR16G16B16A16Sfloat));

    // IRCache main computation pass
    rg.add_pass("ircache_compute",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({gbuffer_depth.geometric_normal, gbuffer_depth.gbuffer, gbuffer_depth.depth, reprojection_map, prev_radiance});
                    pb.writes({ircache_output});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement IRCache compute shader
                            // This would involve:
                            // 1. Binding IRCache compute pipeline
                            // 2. Setting descriptor sets (G-buffer, depth, reprojection, history)
                            // 3. Dispatching compute shader
                            // 4. Irradiance cache computation
                            // 5. Temporal accumulation with reprojection

                            // For now, clear the output
                            vk::ClearColorValue clear_color{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};
                            vk::ImageSubresourceRange range{
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            };
                            cmd.clearColorImage(pb.get_image(ircache_output), vk::ImageLayout::eGeneral, &clear_color, 1, &range);
                        });
                });

    // IRCache filtering pass
    filter_ircache(rg, ircache_output, gbuffer_depth, reprojection_map, ircache_tex_);

    return ircache_output;
}

void IrcacheRenderer::update_eye_position(const glm::vec3& eye_position)
{
    if (!enable_scroll_) {
        return;
    }

    eye_position_ = eye_position;
    grid_center_ = eye_position;

    // Update cascade scroll positions based on eye position
    for (uint32_t cascade = 0; cascade < shared::IRCACHE_CASCADE_COUNT; ++cascade) {
        float cell_diameter = IRCACHE_GRID_CELL_DIAMETER * (1u << cascade);
        glm::vec3 cascade_center = glm::floor(eye_position / cell_diameter);
        glm::ivec3 cascade_origin = glm::ivec3(cascade_center) - glm::ivec3(IRCACHE_CASCADE_SIZE / 2);

        prev_scroll_[cascade] = cur_scroll_[cascade];
        cur_scroll_[cascade] = cascade_origin;

        // Update cascade constants
        glm::ivec3 scroll_amount = cur_scroll_[cascade] - prev_scroll_[cascade];
        ircache_cascades_[cascade].origin = glm::ivec4(cur_scroll_[cascade], 0);
        ircache_cascades_[cascade].voxels_scrolled_this_frame = glm::ivec4(scroll_amount, 0);
    }
}

const std::vector<IrcacheCascadeConstants>& IrcacheRenderer::constants() const
{
    return ircache_cascades_;
}

glm::vec3 IrcacheRenderer::grid_center() const
{
    return grid_center_;
}

void IrcacheRenderer::filter_ircache(render_graph::TemporalRenderGraph& rg, const render_graph::Handle<vulkan::Image>& input,
                               const GbufferDepth& gbuffer_depth,
                               const render_graph::Handle<vulkan::Image>& reprojection_map,
                               PingPongTemporalResource& temporal_tex)
{
    auto [filtered_output, filtered_history] = temporal_tex.get_output_and_history(
        rg, vulkan::ImageDesc::new_2d(rg.resource_desc(input).extent[0],
                                      rg.resource_desc(input).extent[1],
                                      vk::Format::eR16G16B16A16Sfloat));

    // IRCache filtering pass
    rg.add_pass("ircache_filter",
                [&](render_graph::PassBuilder& pb)
                {
                    pb.reads({input, gbuffer_depth.geometric_normal, gbuffer_depth.depth});
                    pb.writes({filtered_output});
                    pb.executes(
                        [&](vk::CommandBuffer cmd)
                        {
                            // TODO: Implement IRCache filtering
                            // This would involve:
                            // 1. Binding IRCache filter compute pipeline
                            // 2. Setting descriptor sets (input, G-buffer, depth)
                            // 3. Dispatching compute shader
                            // 4. Filtering irradiance cache data

                            // For now, copy input to output
                            vk::ImageCopy copy_region{
                                .srcSubresource = {
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .mipLevel = 0,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                },
                                .dstSubresource = {
                                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .mipLevel = 0,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                },
                                .extent = {
                                    .width = rg.resource_desc(input).extent[0],
                                    .height = rg.resource_desc(input).extent[1],
                                    .depth = 1
                                }
                            };
                            cmd.copyImage(pb.get_image(input), vk::ImageLayout::eGeneral,
                                         pb.get_image(filtered_output), vk::ImageLayout::eGeneral,
                                         1, &copy_region);
                        });
                });
}

} // namespace tekki::renderer