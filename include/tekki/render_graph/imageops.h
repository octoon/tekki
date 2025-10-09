#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/Result.h"
#include "RenderGraph.h"
#include "Handle.h"

namespace tekki::render_graph {

class ImageOps {
public:
    /**
     * Clear depth image
     */
    static void ClearDepth(RenderGraph& rg, Handle<Image>& img) {
        try {
            auto& pass = rg.AddPass("clear depth");
            auto output_ref = pass.Write(img, AccessType::TransferWrite);

            pass.Render([output_ref](auto& api) {
                auto& raw_device = api.Device().Raw;
                auto cb = api.cb;

                auto& image = api.Resources().Image(output_ref);

                raw_device.CmdClearDepthStencilImage(
                    cb.raw,
                    image.raw,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ClearDepthStencilValue{
                        .depth = 0.0f,
                        .stencil = 0
                    },
                    vk::ImageSubresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .levelCount = 1,
                        .layerCount = 1
                    }
                );
            });
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("ClearDepth failed: ") + e.what());
        }
    }

    /**
     * Clear color image
     */
    static void ClearColor(RenderGraph& rg, Handle<Image>& img, const glm::vec4& clear_color) {
        try {
            auto& pass = rg.AddPass("clear color");
            auto output_ref = pass.Write(img, AccessType::TransferWrite);

            pass.Render([output_ref, clear_color](auto& api) {
                auto& raw_device = api.Device().Raw;
                auto cb = api.cb;

                auto& image = api.Resources().Image(output_ref);

                raw_device.CmdClearColorImage(
                    cb.raw,
                    image.raw,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ClearColorValue{
                        .float32 = {clear_color.r, clear_color.g, clear_color.b, clear_color.a}
                    },
                    vk::ImageSubresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1
                    }
                );
            });
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("ClearColor failed: ") + e.what());
        }
    }
};

} // namespace tekki::render_graph