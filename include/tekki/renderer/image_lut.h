#pragma once

#include <memory>
#include <vector>
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/render_graph.h"

namespace tekki::renderer {

class ComputeImageLut {
public:
    virtual ~ComputeImageLut() = default;
    virtual std::shared_ptr<tekki::backend::vulkan::Image> Create(std::shared_ptr<tekki::backend::Device> device) = 0;
    virtual void Compute(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph, std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> image) = 0;
};

class ImageLut {
private:
    std::shared_ptr<tekki::backend::vulkan::Image> image;
    std::shared_ptr<ComputeImageLut> computer;
    bool computed;

public:
    ImageLut(std::shared_ptr<tekki::backend::Device> device, std::shared_ptr<ComputeImageLut> computer)
        : computer(computer), computed(false) {
        image = computer->Create(device);
    }

    void ComputeIfNeeded(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph) {
        if (computed) {
            return;
        }

        auto renderGraphImage = renderGraph->Import(image, tekki::backend::vk_sync::AccessType::Nothing);

        computer->Compute(renderGraph, renderGraphImage);

        renderGraph->Export(
            renderGraphImage,
            tekki::backend::vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
        );

        computed = true;
    }

    /// Note: contains garbage until `ComputeIfNeeded` is called.
    std::shared_ptr<tekki::backend::vulkan::Image> BackingImage() const {
        return image;
    }
};

}