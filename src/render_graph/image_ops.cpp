#include "tekki/render_graph/image_ops.h"

namespace tekki {
namespace render_graph {

void clearDepth(RenderGraph& rg, Handle<backend::Image>& img) {
    auto pass = rg.addPass("clear depth");
    auto outputRef = pass.write(img, backend::AccessType::TransferWrite);

    pass.render([outputRef = std::move(outputRef)](RenderPassApi& api) -> Result<void> {
        auto rawDevice = api.device().raw();
        auto cb = api.cb;

        auto image = api.resources().image(outputRef);

        vk::ClearDepthStencilValue clearValue{
            .depth = 0.0f,
            .stencil = 0
        };

        vk::ImageSubresourceRange subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eDepth,
            .levelCount = 1,
            .layerCount = 1
        };

        rawDevice->cmdClearDepthStencilImage(
            cb.raw,
            image.raw(),
            vk::ImageLayout::eTransferDstOptimal,
            clearValue,
            subresourceRange
        );

        return {};
    });
}

void clearColor(RenderGraph& rg, Handle<backend::Image>& img, const std::array<float, 4>& clearColor) {
    auto pass = rg.addPass("clear color");
    auto outputRef = pass.write(img, backend::AccessType::TransferWrite);

    pass.render([outputRef = std::move(outputRef), clearColor](RenderPassApi& api) -> Result<void> {
        auto rawDevice = api.device().raw();
        auto cb = api.cb;

        auto image = api.resources().image(outputRef);

        vk::ClearColorValue clearValue{
            .float32 = clearColor
        };

        vk::ImageSubresourceRange subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1
        };

        rawDevice->cmdClearColorImage(
            cb.raw,
            image.raw(),
            vk::ImageLayout::eTransferDstOptimal,
            clearValue,
            subresourceRange
        );

        return {};
    });
}

} // namespace render_graph
} // namespace tekki