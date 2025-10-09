#pragma once

#include "../render_graph/RenderGraph.h"
#include "../backend/vulkan/device.h"
#include "../backend/vulkan/image.h"

namespace tekki::renderer {

// Base class for LUT computers
class ComputeImageLut {
public:
    virtual ~ComputeImageLut() = default;

    // Create the LUT image
    virtual std::shared_ptr<vulkan::Image> create(vulkan::Device* device) = 0;

    // Compute the LUT contents
    virtual void compute(
        render_graph::RenderGraph& rg,
        render_graph::Handle<vulkan::Image>& img
    ) = 0;
};

// BRDF Fg LUT computer (for PBR lighting)
class BrdfFgLutComputer : public ComputeImageLut {
public:
    BrdfFgLutComputer() = default;

    std::shared_ptr<vulkan::Image> create(vulkan::Device* device) override;

    void compute(
        render_graph::RenderGraph& rg,
        render_graph::Handle<vulkan::Image>& img
    ) override;
};

// Bezold-Brücke LUT computer (for perceptual color shifts)
class BezoldBruckeLutComputer : public ComputeImageLut {
public:
    BezoldBruckeLutComputer() = default;

    std::shared_ptr<vulkan::Image> create(vulkan::Device* device) override;

    void compute(
        render_graph::RenderGraph& rg,
        render_graph::Handle<vulkan::Image>& img
    ) override;
};

} // namespace tekki::renderer
