#include "tekki/renderer/ui_renderer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/rg/temporal_render_graph.h"
#include "tekki/rg/render_graph.h"
#include "tekki/rg/imageops.h"
#include <glm/glm.hpp>
#include <stdexcept>
#include <optional>
#include <memory>
#include <functional>

namespace tekki::renderer {

UiRenderer::UiRenderer() : UiFrame(std::nullopt) {}

/**
 * Prepare render graph for UI rendering
 * @param rg The temporal render graph
 * @return Handle to the rendered image
 */
rg::Handle<Image> UiRenderer::PrepareRenderGraph(rg::TemporalRenderGraph& rg) {
    return RenderUi(rg);
}

/**
 * Render UI to the render graph
 * @param rg The render graph
 * @return Handle to the rendered image
 */
rg::Handle<Image> UiRenderer::RenderUi(rg::RenderGraph& rg) {
    if (UiFrame.has_value()) {
        auto [ui_renderer, image] = std::move(UiFrame.value());
        UiFrame.reset();
        
        auto ui_tex = rg.Import(image, vk::AccessFlagBits::eNone);
        auto pass = rg.AddPass("ui");
        
        pass.Raster(&ui_tex, vk::AccessFlagBits::eColorAttachmentWrite);
        pass.Render([ui_renderer = std::move(ui_renderer)](auto& api) {
            try {
                ui_renderer(api.cb.raw);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("UI render callback failed: ") + e.what());
            }
        });
        
        return ui_tex;
    } else {
        auto blank_img = rg.Create(ImageDesc::New2D(vk::Format::eR8G8B8A8Unorm, {1, 1}));
        std::array<float, 4> clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
        rg::ImageOps::ClearColor(rg, &blank_img, clear_color);
        return blank_img;
    }
}

} // namespace tekki::renderer