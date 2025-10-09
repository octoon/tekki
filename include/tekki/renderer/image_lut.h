#pragma once

#include "../backend/vulkan/image.h"
#include "../render_graph/RenderGraph.h"

#include <memory>

namespace tekki::renderer {

// Interface for computing image LUTs (Look-Up Tables)
class ComputeImageLut {
public:
    virtual ~ComputeImageLut() = default;

    // Create the backing image for the LUT
    virtual std::shared_ptr<vulkan::Image> create(vulkan::Device* device) = 0;

    // Compute the LUT data using a render graph pass
    virtual void compute(render_graph::RenderGraph& rg,
                        const render_graph::Handle<vulkan::Image>& img) = 0;
};

// Manages an image-based LUT with lazy computation
class ImageLut {
public:
    ImageLut(vulkan::Device* device, std::unique_ptr<ComputeImageLut> computer);

    // Compute the LUT if it hasn't been computed yet
    void compute_if_needed(render_graph::RenderGraph& rg);

    // Get the backing image (may contain garbage until compute_if_needed is called)
    std::shared_ptr<vulkan::Image> backing_image() const;

    // Check if the LUT has been computed
    bool is_computed() const { return computed_; }

private:
    std::shared_ptr<vulkan::Image> image_;
    std::unique_ptr<ComputeImageLut> computer_;
    bool computed_;
};

} // namespace tekki::renderer