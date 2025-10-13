#include "tekki/render_graph/imageops.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vk_sync.h"
#include <stdexcept>

namespace tekki::render_graph {

/**
 * Clear depth image
 */
void ImageOps::ClearDepth(RenderGraph& rg, Handle<Image>& img) {
    try {
        PassBuilder pass = rg.AddPass("clear depth");
        auto output_ref = pass.Write(img, vk_sync::AccessType::TransferWrite);

        pass.Render([output_ref](RenderPassApi& api) {
            auto cb = api.cb;

            auto& image = api.Resources().image(output_ref);

            VkClearDepthStencilValue clearValue = {0.0f, 0};
            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            vkCmdClearDepthStencilImage(
                cb.Raw,
                image.raw,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearValue,
                1,
                &range
            );
        });
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ClearDepth failed: ") + e.what());
    }
}

/**
 * Clear color image
 */
void ImageOps::ClearColor(RenderGraph& rg, Handle<Image>& img, const glm::vec4& clear_color) {
    try {
        PassBuilder pass = rg.AddPass("clear color");
        auto output_ref = pass.Write(img, vk_sync::AccessType::TransferWrite);

        pass.Render([output_ref, clear_color](RenderPassApi& api) {
            auto cb = api.cb;

            auto& image = api.Resources().image(output_ref);

            VkClearColorValue clearValue = {};
            clearValue.float32[0] = clear_color.r;
            clearValue.float32[1] = clear_color.g;
            clearValue.float32[2] = clear_color.b;
            clearValue.float32[3] = clear_color.a;

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            vkCmdClearColorImage(
                cb.Raw,
                image.raw,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearValue,
                1,
                &range
            );
        });
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ClearColor failed: ") + e.what());
    }
}

} // namespace tekki::render_graph