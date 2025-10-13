#pragma once

#include <memory>
#include <functional>
#include <vector>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/imageops.h"
#include <vulkan/vulkan.h>

namespace tekki::renderer {

// Forward declarations
namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

class UiRenderer {
public:
    UiRenderer();
    ~UiRenderer() = default;

    /**
     * Used to support Dear Imgui, though could be used for other immediate-mode rendering too
     */
    
    using UiRenderCallback = std::function<void(VkCommandBuffer)>;

    /**
     * Prepare render graph for UI rendering
     * @param rg The temporal render graph
     * @return Handle to the rendered image
     */
    rg::Handle<Image> PrepareRenderGraph(rg::TemporalRenderGraph& rg);

private:
    /**
     * Render UI to the render graph
     * @param rg The render graph
     * @return Handle to the rendered image
     */
    rg::Handle<Image> RenderUi(rg::RenderGraph& rg);

    std::optional<std::pair<UiRenderCallback, std::shared_ptr<Image>>> UiFrame;
};

} // namespace tekki::renderer