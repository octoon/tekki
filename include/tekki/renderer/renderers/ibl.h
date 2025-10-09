#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../render_graph/temporal.h"
#include "../../backend/vulkan/image.h"

#include <vulkan/vulkan.hpp>
#include <optional>
#include <string>

namespace tekki::renderer {

// Image data structure for RGBA16F
struct ImageRgba16f {
    std::array<uint32_t, 2> size;
    std::vector<uint16_t> data;  // f16 data

    ImageRgba16f(uint32_t width, uint32_t height);
    void put_pixel(uint32_t x, uint32_t y, const std::array<uint16_t, 4>& rgba);
};

// IBL (Image-Based Lighting) Renderer
class IblRenderer {
public:
    IblRenderer() = default;

    // Load an environment map from file (EXR or HDR)
    void load_image(const std::string& path);

    // Unload the current environment map
    void unload_image();

    // Render IBL cube map from loaded environment map
    std::optional<render_graph::ReadOnlyHandle<vulkan::Image>> render(
        render_graph::TemporalRenderGraph& rg
    );

private:
    std::optional<ImageRgba16f> image_;
    std::shared_ptr<vulkan::Image> texture_;
};

// Helper functions for loading images
ImageRgba16f load_exr(const std::string& path);
ImageRgba16f load_hdr(const std::string& path);

} // namespace tekki::renderer
