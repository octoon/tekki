#pragma once

#include "../render_graph/RenderGraph.h"
#include "../backend/vulkan/device.h"
#include "../backend/vulkan/image.h"
#include "image_lut.h"

namespace tekki::renderer {

// BRDF Fg LUT computer (for PBR lighting)
class BrdfFgLutComputer : public ComputeImageLut {
public:
    BrdfFgLutComputer() = default;

    std::shared_ptr<vulkan::Image> create(vulkan::Device* device) override;

    void compute(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& img
    ) override;
};

// Bezold-Brücke LUT computer (for perceptual color shifts)
class BezoldBruckeLutComputer : public ComputeImageLut {
public:
    BezoldBruckeLutComputer() = default;

    std::shared_ptr<vulkan::Image> create(vulkan::Device* device) override;

    void compute(
        render_graph::RenderGraph& rg,
        const render_graph::Handle<vulkan::Image>& img
    ) override;
};

} // namespace tekki::renderer
